# dlms-xdlms-client API

## 1. Public Headers

Planned phase 1 headers:

```text
include/dlms/xdlms_client/xdlms_client.hpp
include/dlms/xdlms_client/xdlms_client_status.hpp
include/dlms/xdlms_client/xdlms_client_types.hpp
```

No C ABI is planned for the first implementation.

## 2. Status

`XdlmsClientStatus` shall be a stable status contract:

- `Ok`
- `InvalidArgument`
- `InvalidState`
- `NotAssociated`
- `SendFailed`
- `ReceiveFailed`
- `EncodeFailed`
- `DecodeFailed`
- `InvokeIdMismatch`
- `ServiceRejected`
- `BlockTransferRequired`
- `UnsupportedFeature`
- `InternalError`

## 3. Types

`CosemLogicalName` is a six-byte logical-name value.

`CosemAttributeDescriptor` contains:

- `classId`
- `instanceId`
- `attributeId`

`ServiceOptions` contains:

- `confirmed`
- `highPriority`

The default is confirmed normal priority.

`GetResult` contains:

- `invokeId`
- `hasData`
- `data`
- `hasAccessResult`
- `accessResult`

The first phase keeps `data` as encoded xDLMS data bytes. Typed COSEM data
projection belongs to later service/facade work.

## 4. Client

```cpp
dlms::xdlms_client::XdlmsClient client(channel, association);

dlms::xdlms_client::CosemAttributeDescriptor descriptor = {};
descriptor.classId = 1;
descriptor.instanceId = dlms::xdlms_client::CosemLogicalName(0, 0, 1, 0, 0, 255);
descriptor.attributeId = 2;

dlms::xdlms_client::GetResult result;
const XdlmsClientStatus status = client.Get(descriptor, result);
```

`XdlmsClient` does not own the association object and does not own the profile
APDU channel. The caller must keep both alive for the client lifetime.

## 5. Module Diagram

```mermaid
classDiagram
  class XdlmsClient {
    +Get(CosemAttributeDescriptor, GetResult&) XdlmsClientStatus
  }

  class InvokeIdAllocator {
    +Next() uint8_t
  }

  class CosemAttributeDescriptor {
    +classId
    +instanceId
    +attributeId
  }

  class GetResult {
    +invokeId
    +data
    +accessResult
  }

  XdlmsClient --> InvokeIdAllocator
  XdlmsClient --> CosemAttributeDescriptor
  XdlmsClient --> GetResult
```
