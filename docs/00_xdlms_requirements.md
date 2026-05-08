# dlms-xdlms Requirements

## 1. Scope

The xDLMS layer owns service orchestration after an application association has
already been established. It is the shared boundary for client request flows
and server-side GET/SET/ACTION dispatch contracts.

The layer shall:

- expose high-level LN client service calls for GET, SET, and ACTION;
- define server-side service dispatch contracts for GET, SET, and ACTION;
- use `dlms-association` only as the established association boundary;
- use `dlms-apdu` for xDLMS APDU encode/decode;
- optionally use `dlms-security` to protect and unprotect complete xDLMS APDU
  bytes at the application-layer boundary;
- keep lower profile and transport ownership delegated to lower layers;
- keep COSEM object storage and method execution out of this repo;
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

## 3. Implemented Client Boundary

The current client boundary implements confirmed GET-REQUEST-NORMAL over LN
referencing:

1. The caller provides an associated `AssociationClient` and a profile APDU
   channel boundary.
2. The caller supplies a `CosemAttributeDescriptor`.
3. The client allocates an invoke id, builds GET-REQUEST-NORMAL, and sends it.
4. The client receives one APDU and decodes GET-RESPONSE-NORMAL.
5. A data response is copied into `GetResult`.
6. A data-access-result response is returned as an xDLMS service failure.

## 4. Server-Side Normal GET Boundary

The next implementation phase adds the server-side boundary for
GET-REQUEST-NORMAL and GET-RESPONSE-NORMAL. This boundary does not decode raw
profile frames and does not execute COSEM access decisions itself.

Rules:

- accept an already decoded normal GET request model;
- preserve invoke id, service class, and priority in the response model;
- carry one `CosemAttributeDescriptor`;
- dispatch through an abstract xDLMS server handler owned by the embedding
  server layer;
- return either encoded xDLMS data bytes or a data-access-result;
- map handler failures to xDLMS status codes without throwing exceptions;
- avoid dependency on `dlms-server` or `dlms-cosem` to keep layer direction
  acyclic.

The `dlms-server` repo may later provide an adapter from this abstract handler
contract to the COSEM logical device dispatcher.

## 5. Server-Side Normal GET APDU Boundary

The next APDU boundary shall process already unprotected xDLMS APDU bytes for
GET-REQUEST-NORMAL and produce GET-RESPONSE-NORMAL bytes.

Rules:

- decode exactly one xDLMS APDU from caller-provided bytes;
- accept only GET-REQUEST-NORMAL without selective access;
- reject GET-NEXT, GET-WITH-LIST, block-transfer, selective access, SET,
  ACTION, ciphered APDUs, and ACSE APDUs as unsupported or decode failures;
- derive invoke id, priority, and service-class bits from
  invoke-id-and-priority;
- reject unconfirmed GET requests without producing response bytes;
- dispatch through `XdlmsServerDispatcher`;
- copy the request invoke id and priority into the response;
- encode either xDLMS data bytes from the handler or data-access-result;
- avoid transport, association, security, and COSEM dependencies.

Document RAG alignment:

- the server copies the Invoke_Id into the corresponding response;
- the response carries the same priority flag as the corresponding request;
- unprotected APDU handling is valid only after the association/security layer
  has accepted the request context.

## 6. Server-Side Normal SET Boundary

The next decoded server boundary shall accept SET-REQUEST-NORMAL and produce a
SET-RESPONSE-NORMAL result model. This boundary does not decode raw APDU bytes
and does not write COSEM object storage itself.

Rules:

- accept an already decoded normal SET request model;
- preserve invoke id, service class, and priority in the response model;
- carry one `CosemAttributeDescriptor`;
- carry the SET value as encoded xDLMS `Data` bytes;
- dispatch through the same abstract xDLMS server handler boundary used by
  server-side GET;
- return one data-access-result code for SET-RESPONSE-NORMAL;
- reject missing value bytes before calling the handler;
- keep selective access, SET-WITH-LIST, and SET block transfer out of this
  phase;
- avoid dependency on `dlms-server` or `dlms-cosem` to keep layer direction
  acyclic.

Document RAG alignment:

- SET-REQUEST-NORMAL carries invoke-id-and-priority, a COSEM attribute
  descriptor, an optional access selection field, and a value `Data`;
- SET-RESPONSE-NORMAL carries invoke-id-and-priority and a
  `Data-Access-Result`;
- SET-WITH-FIRST-DATABLOCK, SET-WITH-DATABLOCK, SET-WITH-LIST, and
  SET-WITH-LIST-AND-FIRST-DATABLOCK are separate service shapes and remain out
  of this phase.

## 7. Server-Side Normal SET APDU Boundary

The next APDU boundary shall process already unprotected xDLMS APDU bytes for
SET-REQUEST-NORMAL and produce SET-RESPONSE-NORMAL bytes.

Rules:

- decode exactly one xDLMS APDU from caller-provided bytes;
- accept only SET-REQUEST-NORMAL without selective access;
- reject SET-WITH-FIRST-DATABLOCK, SET-WITH-DATABLOCK, SET-WITH-LIST,
  SET-WITH-LIST-AND-FIRST-DATABLOCK, selective access, GET, ACTION, ciphered
  APDUs, and ACSE APDUs as unsupported or decode failures;
- derive invoke id, priority, and service-class bits from
  invoke-id-and-priority;
- reject unconfirmed SET requests without producing response bytes;
- re-encode the SET value as xDLMS `Data` bytes for `SetIndication::data`;
- dispatch through `XdlmsServerDispatcher`;
- copy the request invoke id and priority into the response;
- encode SET-RESPONSE-NORMAL with the handler data-access-result;
- avoid transport, association, security, and COSEM dependencies.

Document RAG alignment:

- the SET service may be confirmed or unconfirmed, but this phase supports
  confirmed normal SET only;
- the SET service writes one or more COSEM object attributes; this phase
  supports one attribute in one request;
- large SET payloads use block-transfer request types and remain out of scope.

## 8. State Requirements

The client implementation shall be stateless except for invoke-id allocation.
The server boundary shall be stateless and store only non-owning handler
references.

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

## 9. Security Boundary

The xDLMS layer may optionally protect outbound APDUs and unprotect inbound
APDUs through `dlms-security`. The cryptographic policy, keys, invocation
counters, and system titles remain owned by `dlms-security`; `dlms-xdlms` only
decides when a complete xDLMS APDU byte vector crosses the security boundary.

Rules:

- client GET/SET/ACTION first encode an unprotected xDLMS request APDU through
  `dlms-apdu`;
- when configured, the client passes the encoded request to
  `CipheredApduProcessor::Protect()` before sending it to `IApduChannel`;
- when configured, the client passes the received response to
  `CipheredApduProcessor::Unprotect()` before decoding it through `dlms-apdu`;
- server APDU processing unprotects the request before xDLMS decode and
  protects the encoded response before returning it to the caller;
- no-security construction keeps the existing unprotected behavior;
- security failures map to a dedicated xDLMS status and do not close or release
  an association.

Document RAG alignment:

- DLMS/COSEM builds the unprotected service APDU first, then builds the
  ciphered APDU when protection is required;
- on receive, the application layer deciphers the APDU and restores the
  original unprotected APDU before invoking the service primitive;
- ciphered APDUs carry security control information, invocation counter,
  encrypted APDU bytes, and authentication tag.

## 10. Out of Scope

- association opening and release;
- Wrapper, HDLC, LLC, TCP, UDP, and serial transport ownership;
- server-side ACTION dispatch implementation;
- server-side SET selective access, list, and block-transfer forms;
- server-side GET-NEXT, GET-WITH-LIST, and block transfer;
- COSEM object access implementation inside `dlms-xdlms`;
- GET-WITH-LIST;
- GET-NEXT and block transfer;
- selective access parameters;
- COSEM object registry and access-right decisions;
- public facade connection options.
