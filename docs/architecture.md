# dlms-xdlms Architecture

## 1. Layer Position

```mermaid
flowchart TD
  Facade["Future dlms-client"]
  Server["Future dlms-server"]
  XDlms["lib/dlms-xdlms"]
  Association["lib/dlms-association"]
  Apdu["lib/dlms-apdu"]
  Profile["lib/dlms-profile"]
  Cosem["lib/dlms-cosem"]

  Facade --> XDlms
  Server --> XDlms
  XDlms --> Association
  XDlms --> Apdu
  XDlms --> Profile
  XDlms --> Cosem
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
    +DispatchGet() XdlmsStatus
    +DispatchSet() XdlmsStatus
    +DispatchAction() XdlmsStatus
  }

  class CosemAccessPort {
    +ReadAttribute()
    +WriteAttribute()
    +InvokeMethod()
  }

  class dlms_apdu {
    +EncodeXdlmsApdu()
    +DecodeXdlmsApdu()
  }

  XdlmsClient --> AssociationClient
  XdlmsClient --> IApduChannel
  XdlmsClient --> InvokeIdAllocator
  XdlmsClient --> dlms_apdu
  XdlmsServerDispatcher --> CosemAccessPort
  XdlmsServerDispatcher --> dlms_apdu
```

## 4. Ownership

`XdlmsClient` stores non-owning references to the association and profile APDU
channel boundaries. Server dispatch stores non-owning access to a COSEM access
port. The layer does not own transport resources, association lifetime, or
COSEM object storage.

## 5. Error Model

The layer returns status codes only. Runtime API paths do not throw exceptions.
Failures are reported at the xDLMS service boundary and do not close or release
the association.
