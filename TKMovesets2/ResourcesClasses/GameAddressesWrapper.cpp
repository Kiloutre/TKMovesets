#include "GameAddressesWrapper.hpp"

// -- Game addresses interface -- //

int64_t GameAddressesWrapper::GetValue(const char* c_addressId)
{
	try {
		return addrFile->GetValue(minorGameKey, c_addressId);
	}
	catch (GameAddressNotFound& e) {
		return addrFile->GetValue(gameKey, c_addressId);
	}
}

const char* GameAddressesWrapper::GetString(const char* c_addressId)
{
	try {
		return addrFile->GetString(minorGameKey, c_addressId);
	}
	catch (GameAddressNotFound& e) {
		return addrFile->GetString(gameKey, c_addressId);
	}
}

const std::vector<gameAddr>& GameAddressesWrapper::GetAddress(const char* c_addressId, bool& isRelative)
{
	try {
		return addrFile->GetAddress(minorGameKey, c_addressId, isRelative);
	}
	catch (GameAddressNotFound& e) {
		return addrFile->GetAddress(gameKey, c_addressId, isRelative);
	}
}