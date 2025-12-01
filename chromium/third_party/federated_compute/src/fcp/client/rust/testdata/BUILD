package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "endorsement",
    srcs = [
        "endorsement.json",
        "endorsement.json.sig",
        "logentry.json",
        # Oak's tr_test_verifying_key
        "endorser_public_key.pem",
        # Public key of the Rekor instance hosted by sigstore.dev. It is downloaded
        # from https://rekor.sigstore.dev/api/v1/log/publicKey.
        "rekor_public_key.pem",
    ],
)
