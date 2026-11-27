#!/usr/bin/env python3
"""r2_upload.py: upload one file to the R2 bucket of the content packs.

    python -I tools/r2_upload.py FILE [--key NAME] [--bucket ascent-content]
                                 [--env FILE] [--part-mb 64]

The upload is an S3 multipart upload (wrangler takes files up to 300 MiB
only), signed with AWS Signature Version 4, to
https://<CLOUDFLARE_ACCOUNT_ID>.r2.cloudflarestorage.com (region "auto").
The keys come from the env file (default
D:/dev/google cloud/secrets/cloudflare.env: CLOUDFLARE_ACCOUNT_ID,
R2_ACCESS_KEY_ID, R2_SECRET_ACCESS_KEY). The tool never prints them.

Upload a pack before manifest.txt, never after it. Then check the pack with
`ascent-content download PACK FILE` (docs/install.md).

Needs the Python package requests.
"""
import argparse
import datetime
import hashlib
import hmac
import os
import re
import sys
import urllib.parse

import requests

DEFAULT_ENV = "D:/dev/google cloud/secrets/cloudflare.env"


def read_env(path):
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            m = re.match(r"\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$", line)
            if m:
                out[m.group(1)] = m.group(2).strip("\"'")
    return out


def sign(key, msg):
    return hmac.new(key, msg.encode("utf-8"), hashlib.sha256).digest()


class R2:
    def __init__(self, account, access, secret, bucket):
        self.host = "%s.r2.cloudflarestorage.com" % account
        self.access, self.secret, self.bucket = access, secret, bucket
        self.s = requests.Session()

    def request(self, method, key, query=None, body=b"", headers=None):
        query = query or {}
        now = datetime.datetime.now(datetime.timezone.utc)
        amz, day = now.strftime("%Y%m%dT%H%M%SZ"), now.strftime("%Y%m%d")
        path = "/" + self.bucket + "/" + urllib.parse.quote(key, safe="/-_.~")
        cq = "&".join("%s=%s" % (urllib.parse.quote(k, safe="-_.~"), urllib.parse.quote(str(v), safe="-_.~"))
                      for k, v in sorted(query.items()))
        sha = hashlib.sha256(body).hexdigest()
        h = {"host": self.host, "x-amz-content-sha256": sha, "x-amz-date": amz}
        for k, v in (headers or {}).items():
            h[k.lower()] = v
        names = ";".join(sorted(h))
        creq = "\n".join([method, path, cq, "".join("%s:%s\n" % (k, h[k]) for k in sorted(h)), names, sha])
        scope = "%s/auto/s3/aws4_request" % day
        to_sign = "\n".join(["AWS4-HMAC-SHA256", amz, scope, hashlib.sha256(creq.encode("utf-8")).hexdigest()])
        k = sign(sign(sign(sign(("AWS4" + self.secret).encode("utf-8"), day), "auto"), "s3"), "aws4_request")
        sig = hmac.new(k, to_sign.encode("utf-8"), hashlib.sha256).hexdigest()
        h["authorization"] = "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s" % (
            self.access, scope, names, sig)
        del h["host"]
        url = "https://%s%s%s" % (self.host, path, "?" + cq if cq else "")
        r = self.s.request(method, url, data=body, headers=h, timeout=600)
        if r.status_code >= 300:
            raise SystemExit("r2_upload: %s %s: HTTP %d: %s" % (method, key, r.status_code, r.text[:300]))
        return r


def main():
    p = argparse.ArgumentParser()
    p.add_argument("file")
    p.add_argument("--key")
    p.add_argument("--bucket", default="ascent-content")
    p.add_argument("--env", default=DEFAULT_ENV)
    p.add_argument("--part-mb", type=int, default=64)
    p.add_argument("--content-type", default=None)
    a = p.parse_args()
    env = read_env(a.env)
    for n in ("CLOUDFLARE_ACCOUNT_ID", "R2_ACCESS_KEY_ID", "R2_SECRET_ACCESS_KEY"):
        if not env.get(n):
            raise SystemExit("r2_upload: %s is not in the env file" % n)
    key = a.key or os.path.basename(a.file)
    ctype = a.content_type or ("text/plain; charset=utf-8" if key.endswith(".txt") else "application/zip" if key.endswith(".zip")
                               else "application/octet-stream")
    r2 = R2(env["CLOUDFLARE_ACCOUNT_ID"], env["R2_ACCESS_KEY_ID"], env["R2_SECRET_ACCESS_KEY"], a.bucket)
    size = os.path.getsize(a.file)
    part = a.part_mb * 1024 * 1024
    if size <= part:
        with open(a.file, "rb") as f:
            r2.request("PUT", key, body=f.read(), headers={"content-type": ctype})
        print("r2_upload: %s: %d bytes in one request" % (key, size))
        return 0
    r = r2.request("POST", key, query={"uploads": ""}, headers={"content-type": ctype})
    upload = re.search(r"<UploadId>([^<]+)</UploadId>", r.text).group(1)
    etags = []
    try:
        with open(a.file, "rb") as f:
            n = 1
            while True:
                data = f.read(part)
                if not data:
                    break
                r = r2.request("PUT", key, query={"partNumber": n, "uploadId": upload}, body=data)
                etags.append((n, r.headers["ETag"]))
                print("r2_upload: %s: part %d, %d of %d bytes" % (key, n, min(n * part, size), size), flush=True)
                n += 1
        body = "<CompleteMultipartUpload>%s</CompleteMultipartUpload>" % "".join(
            "<Part><PartNumber>%d</PartNumber><ETag>%s</ETag></Part>" % e for e in etags)
        r2.request("POST", key, query={"uploadId": upload}, body=body.encode("utf-8"))
    except BaseException:
        try:
            r2.request("DELETE", key, query={"uploadId": upload})
        except SystemExit:
            pass
        raise
    print("r2_upload: %s: %d bytes in %d parts" % (key, size, len(etags)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
