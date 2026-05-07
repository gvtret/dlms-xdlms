# dlms-xdlms Architecture

## 1. Layer Position

```mermaid
flowchart TD
  Facade["Future dlms-client"]
  Server["dlms-server adapter"]
  XDlms["lib/dlms-xdlms"]
  Association["lib/dlms-association"]
  Apdu["lib/dlms-apdu"]
  Profile["lib/dlms-profile"]

  Facade --> XDlms
  Server --> XDlms
  XDlms --> Association
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

## 7. Ownership

`XdlmsClient` stores non-owning references to the association and profile APDU
channel boundaries. Server dispatch stores non-owning access to an xDLMS server
handler. The layer does not own transport resources, association lifetime, or
COSEM object storage.

## 8. Error Model

The layer returns status codes only. Runtime API paths do not throw exceptions.
Failures are reported at the xDLMS service boundary and do not close or release
the association.
