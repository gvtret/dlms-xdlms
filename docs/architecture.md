# dlms-xdlms Architecture

## 1. Layer Position

```mermaid
flowchart TD
  Facade["Future dlms-client"]
  Server["dlms-server adapter"]
  XDlms["lib/dlms-xdlms"]
  Association["lib/dlms-association"]
  Security["lib/dlms-security"]
  Apdu["lib/dlms-apdu"]
  Profile["lib/dlms-profile"]

  Facade --> XDlms
  Server --> XDlms
  XDlms --> Association
  XDlms --> Security
  XDlms --> Apdu
  XDlms --> Profile
```

## 2. Normal GET Flow

```mermaid
sequenceDiagram
  participant App as Caller
  participant Client as XdlmsClient
  participant Assoc as AssociationClient
  participant Apdu as dlms-apdu
  participant Channel as IApduChannel

  App->>Client: Get(descriptor)
  Client->>Assoc: IsAssociated()
  Client->>Client: Allocate invoke id
  Client->>Apdu: Encode GET-REQUEST-NORMAL
  Client->>Channel: SendApdu(request)
  Client->>Channel: ReceiveApdu()
  Client->>Apdu: Decode GET-RESPONSE
  Client-->>App: GetResult
```

## 3. Class Interaction

```mermaid
classDiagram
  class XdlmsClient {
    +Get(CosemAttributeDescriptor, GetResult&) XdlmsStatus
  }

  class AssociationClient {
    +IsAssociated() bool
  }

  class IApduChannel {
    +SendApdu(ProfileByteView) ProfileStatus
    +ReceiveApdu(vector~uint8_t~&) ProfileStatus
  }

  class InvokeIdAllocator {
    +Next() uint8_t
  }

  class XdlmsServerDispatcher {
    +DispatchGet(GetIndication, GetResult&) XdlmsStatus
  }

  class IXdlmsServerHandler {
    +HandleGet(GetIndication, GetResult&) XdlmsStatus
  }

  class GetIndication {
    +invokeId
    +options
    +descriptor
  }

  class dlms_apdu {
    +EncodeXdlmsApdu()
    +DecodeXdlmsApdu()
  }

  XdlmsClient --> AssociationClient
  XdlmsClient --> IApduChannel
  XdlmsClient --> InvokeIdAllocator
  XdlmsClient --> dlms_apdu
  XdlmsServerDispatcher --> IXdlmsServerHandler
  XdlmsServerDispatcher --> GetIndication
  XdlmsServerDispatcher --> GetResult
```

## 4. Server Normal GET Flow

```mermaid
sequenceDiagram
  participant Apdu as APDU Decoder Boundary
  participant Dispatcher as XdlmsServerDispatcher
  participant Handler as IXdlmsServerHandler
  participant Server as dlms-server Adapter

  Apdu->>Dispatcher: DispatchGet(indication)
  Dispatcher->>Dispatcher: Validate invoke id and descriptor
  Dispatcher->>Handler: HandleGet(indication, result)
  Handler->>Server: Read attribute through server model
  Server-->>Handler: encoded data or access result
  Handler-->>Dispatcher: XdlmsStatus
  Dispatcher-->>Apdu: GetResult with matching invoke id
```

`dlms-xdlms` owns the xDLMS request and response shape. The embedding server
layer owns association policy, COSEM object access, and access-right decisions.

## 5. Server APDU GET Flow

```mermaid
sequenceDiagram
  participant Caller as Profile or Association Boundary
  participant Processor as XdlmsServerApduProcessor
  participant Apdu as dlms-apdu
  participant Dispatcher as XdlmsServerDispatcher
  participant Handler as IXdlmsServerHandler

  Caller->>Processor: ProcessRequest(requestApdu)
  Processor->>Apdu: DecodeXdlmsApdu
  Processor->>Processor: Require GET-REQUEST-NORMAL
  Processor->>Dispatcher: DispatchGet(indication)
  Dispatcher->>Handler: HandleGet(indication, result)
  Handler-->>Dispatcher: data or data-access-result
  Dispatcher-->>Processor: GetResult
  Processor->>Apdu: Encode GET-RESPONSE-NORMAL
  Processor-->>Caller: responseApdu
```

The processor handles xDLMS service bytes only. Profile framing, ACSE
association state, and ciphered APDU protection are still owned by adjacent
layers.

## 6. Server Normal SET Flow

```mermaid
sequenceDiagram
  participant Boundary as Decoded SET Boundary
  participant Dispatcher as XdlmsServerDispatcher
  participant Handler as IXdlmsServerHandler
  participant Server as dlms-server Adapter

  Boundary->>Dispatcher: DispatchSet(indication)
  Dispatcher->>Dispatcher: Validate invoke id, descriptor, and value bytes
  Dispatcher->>Handler: HandleSet(indication, result)
  Handler->>Server: Write attribute through server model
  Server-->>Handler: data-access-result
  Handler-->>Dispatcher: XdlmsStatus
  Dispatcher-->>Boundary: SetResult with matching invoke id
```

`dlms-xdlms` owns the SET service contract and result shape. The embedding
server layer owns write authorization, object lookup, and actual attribute
mutation.

## 7. Server APDU SET Flow

```mermaid
sequenceDiagram
  participant Caller as Profile or Association Boundary
  participant Processor as XdlmsServerApduProcessor
  participant Apdu as dlms-apdu
  participant Dispatcher as XdlmsServerDispatcher
  participant Handler as IXdlmsServerHandler

  Caller->>Processor: ProcessRequest(requestApdu)
  Processor->>Apdu: DecodeXdlmsApdu
  Processor->>Processor: Require SET-REQUEST-NORMAL
  Processor->>Processor: Re-encode request value Data bytes
  Processor->>Dispatcher: DispatchSet(indication)
  Dispatcher->>Handler: HandleSet(indication, result)
  Handler-->>Dispatcher: data-access-result
  Dispatcher-->>Processor: SetResult
  Processor->>Apdu: Encode SET-RESPONSE-NORMAL
  Processor-->>Caller: responseApdu
```

The processor does not evaluate write permissions and does not decode the
semantic COSEM value. It only preserves the xDLMS `Data` bytes across the APDU
and decoded-dispatch boundary.

## 8. Security APDU Boundary

```mermaid
sequenceDiagram
  participant App as Caller
  participant Client as XdlmsClient
  participant Security as CipheredApduProcessor
  participant Channel as IApduChannel

  App->>Client: Get/Set/Action
  Client->>Client: Encode unprotected xDLMS APDU
  Client->>Security: Protect(requestApdu)
  Security-->>Client: ciphered request APDU
  Client->>Channel: SendApdu(ciphered request)
  Channel-->>Client: ciphered response APDU
  Client->>Security: Unprotect(responseApdu)
  Security-->>Client: unprotected response APDU
  Client->>Client: Decode xDLMS response
  Client-->>App: service result
```

```mermaid
sequenceDiagram
  participant Caller as Association or Server Boundary
  participant Processor as XdlmsServerApduProcessor
  participant Security as CipheredApduProcessor
  participant Dispatcher as XdlmsServerDispatcher

  Caller->>Processor: ProcessRequest(ciphered request)
  Processor->>Security: Unprotect(requestApdu)
  Security-->>Processor: unprotected request APDU
  Processor->>Processor: Decode and validate xDLMS service
  Processor->>Dispatcher: Dispatch indication
  Dispatcher-->>Processor: service result
  Processor->>Processor: Encode unprotected response APDU
  Processor->>Security: Protect(responseApdu)
  Security-->>Processor: ciphered response APDU
  Processor-->>Caller: ciphered response
```

The xDLMS layer remains a consumer of `dlms-security`. It does not choose
keys, maintain invocation counters directly, build system titles, or implement
AES-GCM. It only preserves the DLMS/COSEM ordering: service APDU first,
ciphered APDU at the application-layer boundary.

## 9. Client GET Block Transfer

```mermaid
sequenceDiagram
  participant App as Caller
  participant Client as XdlmsClient
  participant Blocks as BlockTransferManager
  participant Channel as IApduChannel

  App->>Client: Get(descriptor)
  Client->>Channel: Send GET-REQUEST-NORMAL
  Channel-->>Client: GET-RESPONSE-WITH-DATABLOCK block 1, last=false
  Client->>Blocks: Append block 1
  Client->>Channel: Send GET-REQUEST-NEXT block 1
  Channel-->>Client: GET-RESPONSE-WITH-DATABLOCK block 2, last=true
  Client->>Blocks: Append block 2
  Client-->>App: GetResult with concatenated raw-data
```

The block manager is scoped to one synchronous GET call. It validates block
numbers, enforces the collected-size limit, and returns the final raw-data
bytes to the existing `GetResult` contract.

## 9.1 Server GET Response Block Transfer

```mermaid
sequenceDiagram
  participant Client as Client
  participant Server as XdlmsServerApduProcessor
  participant Handler as IXdlmsServerHandler

  Client->>Server: GET-REQUEST-NORMAL
  Server->>Handler: HandleGet()
  Handler-->>Server: GetResult(data = large value)
  Server-->>Client: GET-RESPONSE-WITH-DATABLOCK block 1
  Client->>Server: GET-REQUEST-NEXT block 1
  Server-->>Client: GET-RESPONSE-WITH-DATABLOCK block 2, last=true
```

`XdlmsServerApduProcessor` owns one `GetResponseBlockState` alongside the
existing SET and ACTION request block states. The state stores the complete
encoded response data, the next block number, and the current offset. It is
cleared after the final block or on decode/invoke mismatch failures.

## 10. Client SET Block Transfer

```mermaid
sequenceDiagram
  participant App as Caller
  participant Client as XdlmsClient
  participant Channel as IApduChannel

  App->>Client: Set(descriptor, encodedData, options)
  Client->>Channel: SET-REQUEST-WITH-FIRST-DATABLOCK block 1
  Channel-->>Client: SET-RESPONSE-DATABLOCK block 1
  Client->>Channel: SET-REQUEST-WITH-DATABLOCK block 2, last=true
  Channel-->>Client: SET-RESPONSE-LAST-DATABLOCK block 2, result=success
  Client-->>App: SetResult
```

The SET block sender is scoped to one synchronous SET call. It reuses the same
invoke-id-and-priority byte for all blocks, validates acknowledged block
numbers, and returns the final data-access-result through the existing
`SetResult` contract.

## 11. Client ACTION Block Transfer

```mermaid
sequenceDiagram
  participant App as Caller
  participant Client as XdlmsClient
  participant Blocks as ActionResponseBlockCollector
  participant Channel as IApduChannel

  App->>Client: Action(descriptor, parameter)
  Client->>Channel: ACTION-REQUEST-NORMAL
  Channel-->>Client: ACTION-RESPONSE-WITH-PBLOCK block 1, last=false
  Client->>Blocks: Append block 1
  Client->>Channel: ACTION-REQUEST-NEXT-PBLOCK block 1
  Channel-->>Client: ACTION-RESPONSE-WITH-PBLOCK block 2, last=true
  Client->>Blocks: Append block 2
  Client->>Blocks: Decode action response payload
  Client-->>App: ActionResult
```

ACTION block transfer is kept separate from GET and SET because the service can
block both request invocation parameters and response return parameters.

## 12. Block State Machine

```mermaid
stateDiagram-v2
  [*] --> Idle

  Idle --> ClientGetCollecting: client receives GET response block\nlast=false
  ClientGetCollecting --> ClientGetCollecting: next GET response block\nlast=false and expected block
  ClientGetCollecting --> Idle: next GET response block\nlast=true
  ClientGetCollecting --> Idle: send/receive/decode/block error

  Idle --> ClientSetSending: SET payload exceeds request block limit
  ClientSetSending --> ClientSetSending: SET block ack matches expected block
  ClientSetSending --> Idle: SET last block response accepted
  ClientSetSending --> Idle: send/receive/decode/block error

  Idle --> ClientActionRequestSending: ACTION parameter exceeds request block limit
  ClientActionRequestSending --> ClientActionRequestSending: ACTION request pblock ack matches expected block
  ClientActionRequestSending --> ClientActionResponseCollecting: final request block accepted\nresponse starts pblocks
  ClientActionRequestSending --> Idle: final normal ACTION response accepted
  ClientActionRequestSending --> Idle: send/receive/decode/block error

  Idle --> ClientActionResponseCollecting: client receives ACTION response pblock\nlast=false
  ClientActionResponseCollecting --> ClientActionResponseCollecting: next ACTION response pblock\nlast=false and expected block
  ClientActionResponseCollecting --> Idle: next ACTION response pblock\nlast=true
  ClientActionResponseCollecting --> Idle: send/receive/decode/block error

  Idle --> ServerSetRequestActive: SET first data block\nlast=false
  ServerSetRequestActive --> ServerSetRequestActive: following SET data block\nlast=false and expected block
  ServerSetRequestActive --> Idle: following SET data block\nlast=true
  ServerSetRequestActive --> Idle: malformed, skipped, duplicate, or invoke mismatch

  Idle --> ServerActionRequestActive: ACTION first pblock\nlast=false
  ServerActionRequestActive --> ServerActionRequestActive: following ACTION pblock\nlast=false and expected block
  ServerActionRequestActive --> Idle: following ACTION pblock\nlast=true
  ServerActionRequestActive --> Idle: malformed, skipped, duplicate, or invoke mismatch

  Idle --> ServerGetResponseActive: oversized successful GET result
  ServerGetResponseActive --> ServerGetResponseActive: GET-REQUEST-NEXT matches latest sent block
  ServerGetResponseActive --> Idle: final GET response block sent
  ServerGetResponseActive --> Idle: missing state, wrong block, or invoke mismatch
```

Client block states are scoped to one synchronous service call. Server block
states are scoped to one `XdlmsServerApduProcessor` instance and one active
association/session. Each service direction owns independent state so SET
request reassembly, ACTION request reassembly, and GET response production do
not share buffers.

## 13. Ownership

`XdlmsClient` stores non-owning references to the association and profile APDU
channel boundaries and may store a non-owning reference to a security
processor. Server dispatch stores non-owning access to an xDLMS server handler.
The server APDU processor may also store a non-owning security processor
reference. The layer does not own transport resources, association lifetime,
security material, or COSEM object storage. Block-transfer state is limited to
one client service call or one active server APDU processor sequence.

## 14. Error Model

The layer returns status codes only. Runtime API paths do not throw exceptions.
Failures are reported at the xDLMS service boundary and do not close or release
the association.
