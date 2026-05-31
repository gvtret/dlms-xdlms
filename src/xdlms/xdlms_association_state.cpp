#include "dlms/xdlms/xdlms_association_state.hpp"

#include "dlms/association/association_client.hpp"

namespace dlms {
namespace xdlms {

IXdlmsAssociationState::~IXdlmsAssociationState()
{
}

AssociationClientXdlmsAssociationState::AssociationClientXdlmsAssociationState(
  dlms::association::AssociationClient& association)
  : association_(association)
{
}

bool AssociationClientXdlmsAssociationState::IsAssociated() const
{
  return association_.IsAssociated();
}

} // namespace xdlms
} // namespace dlms
