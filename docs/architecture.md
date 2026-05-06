# dlms-xdlms-client Architecture

## 1. Layer Position

```mermaid
flowchart TD
  Facade["Future dlms-client"]
  XClient["lib/dlms-xdlms-client"]
  Association["lib/dlms-association"]
  Apdu["lib/dlms-apdu"]
  Profile["lib/dlms-profile"]

  Facade --> XClient
  XClient --> Association
  XClient --> Apdu
  XClient --> Profile
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
    +Get(CosemAttributeDescriptor, GetResult&) XdlmsClientStatus
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

  class dlms_apdu {
    +EncodeXdlmsApdu()
    +DecodeXdlmsApdu()
  }

  XdlmsClient --> AssociationClient
  XdlmsClient --> IApduChannel
  XdlmsClient --> InvokeIdAllocator
  XdlmsClient --> dlms_apdu
```

## 4. Ownership

`XdlmsClient` stores non-owning references to the association and profile APDU
channel boundaries. It does not own transport resources, association lifetime,
or COSEM object storage.

## 5. Error Model

The layer returns status codes only. Runtime API paths do not throw exceptions.
Failures are reported at the xDLMS service boundary and do not close or release
the association.
