# dlms-xdlms-client Requirements

## 1. Scope

The xDLMS client layer owns client-side service orchestration after an
application association has already been established.

The layer shall:

- expose high-level LN service calls for GET, then later SET and ACTION;
- use `dlms-association` only as the established association boundary;
- use `dlms-apdu` for xDLMS APDU encode/decode;
- keep lower profile and transport ownership delegated to lower layers;
- keep COSEM object storage and server dispatch out of this repo;
- keep public ergonomic connection setup out of this repo.

## 2. Normative Model

Document RAG references:

- Green Book edition 8.3 describes GET as the LN service used to read one or
  more COSEM object attributes.
- A GET-REQUEST-NORMAL carries one COSEM attribute descriptor.
- The descriptor contains the COSEM class id, logical-name instance id, and
  attribute id.
- GET can be confirmed or unconfirmed, but the v1 client uses confirmed
  normal GET.
- A normal response may carry either a data value or a data-access-result.
- Block transfer is used when a response does not fit in one APDU; this is out
  of scope for the first implementation.

## 3. First Implementation

Version 1 implements only confirmed GET-REQUEST-NORMAL over LN referencing:

1. The caller provides an associated `AssociationClient` and a profile APDU
   channel boundary.
2. The caller supplies a `CosemAttributeDescriptor`.
3. The client allocates an invoke id, builds GET-REQUEST-NORMAL, and sends it.
4. The client receives one APDU and decodes GET-RESPONSE-NORMAL.
5. A data response is copied into `GetResult`.
6. A data-access-result response is returned as an xDLMS service failure.

## 4. State Requirements

The first implementation shall be stateless except for invoke-id allocation.

Rules:

- service calls require an already associated association;
- service calls must not call association `Open()` or `Establish()`;
- invoke ids are stable 4-bit values and wrap within the xDLMS range;
- a send failure returns `SendFailed`;
- a receive failure returns `ReceiveFailed`;
- malformed or unexpected APDUs return `DecodeFailed`;
- response invoke-id mismatch returns `InvokeIdMismatch`;
- block-transfer responses return `BlockTransferRequired` until a block
  manager is implemented.

## 5. Security Boundary

The v1 layer shall not protect or unprotect ciphered APDUs. Security options
may be modeled later, but actual ciphering belongs in `dlms-security`.

## 6. Out of Scope

- association opening and release;
- Wrapper, HDLC, LLC, TCP, UDP, and serial transport ownership;
- SET and ACTION services;
- GET-WITH-LIST;
- GET-NEXT and block transfer;
- selective access parameters;
- ciphered xDLMS APDUs;
- COSEM object registry and access-right decisions;
- public facade connection options.
