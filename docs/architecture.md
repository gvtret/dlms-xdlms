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

## 11. Ownership

`XdlmsClient` stores non-owning references to the association and profile APDU
channel boundaries and may store a non-owning reference to a security
processor. Server dispatch stores non-owning access to an xDLMS server handler.
The server APDU processor may also store a non-owning security processor
reference. The layer does not own transport resources, association lifetime,
security material, COSEM object storage, or block-transfer persistence outside
one service call.

## 12. Error Model

The layer returns status codes only. Runtime API paths do not throw exceptions.
Failures are reported at the xDLMS service boundary and do not close or release
the association.
