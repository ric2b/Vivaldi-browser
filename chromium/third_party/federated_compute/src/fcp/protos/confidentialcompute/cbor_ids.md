# CBOR Ids

The APIs defined in this directory introduce the following CBOR (RFC 8949) ids
used for CBOR Web Token (CWT; RFC 8392) and CBOR Object Signing and Encryption
(COSE; RFC 9052) objects.

## COSE Header Parameters

label  | type | description
------ | ---- | -----------
-65537 | bstr | HPKE encapsulated key. Semantics match `ek` in https://datatracker.ietf.org/doc/draft-ietf-cose-hpke/11/.
-65538 | bstr / null | required logical pipeline state in order to decrypt a COSE_Encrypt structure. If null, there must be no existing state.
-65539 | bstr | new logical pipeline state to store when decrypting a COSE_Encrypt structure

## COSE Algorithms

value  | description
------ | ---------------------------------------------------------------
-65537 | HPKE-Base-X25519-SHA256-AES128GCM
-65538 | AEAD_AES_128_GCM_SIV (fixed nonce: x"74DF8FD4BE34AF647F5E54F6")

## CWT Claims

key    | type | description
------ | ---- | -----------
-65537 | bstr | COSE_Key containing the public key parameters
-65538 | bstr | google.protobuf.Struct containing configuration-derived properties. An absent claim should be treated like an empty Struct. Field names may not contain `.` or `*` characters.
-65539 | tstr | name of the logical pipeline to which a transform belongs
-65540 | bstr | id of the pipeline invocation to which a transform belongs
-65541 | uint | index of the Transform message in an fcp.confidentialcompute.PipelineVariantPolicy that a transform matched
-65542 | [+ uint] | ids of the output nodes a transform is authorized to produce. This matches dst_node_ids in the fcp.confidentialcompute.PipelineVariantPolicy's Transform
-65543 | bstr | SHA-256 hash of the access policy used by the KMS to derive an encryption key
