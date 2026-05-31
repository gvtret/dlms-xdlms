#pragma once

namespace dlms {
namespace association {
class AssociationClient;
}
namespace xdlms {

class IXdlmsAssociationState
{
public:
  virtual ~IXdlmsAssociationState();

  virtual bool IsAssociated() const = 0;
};

class AssociationClientXdlmsAssociationState : public IXdlmsAssociationState
{
public:
  explicit AssociationClientXdlmsAssociationState(
    dlms::association::AssociationClient& association);

  bool IsAssociated() const;

private:
  dlms::association::AssociationClient& association_;
};

} // namespace xdlms
} // namespace dlms
