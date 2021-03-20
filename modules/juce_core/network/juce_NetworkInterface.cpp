/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

	NetworkInterface::NetworkInterface(const NetworkInterface& other) noexcept
	{
		this->deviceName = other.deviceName;
		this->friendlyName = other.friendlyName;
		this->index = other.index;
		this->macAddress = other.macAddress;
		this->allIpAddresses = other.allIpAddresses;
		this->ipv4Addresses = other.ipv4Addresses;
		this->mtuSize = other.mtuSize;
		this->rxSpeed = other.rxSpeed;
		this->txSpeed = other.txSpeed;
		this->interfaceUp = other.interfaceUp;
	}

	NetworkInterface& NetworkInterface::operator= (const NetworkInterface& other) noexcept
	{
		this->deviceName = other.deviceName;
		this->friendlyName = other.friendlyName;
		this->index = other.index;
		this->macAddress = other.macAddress;
		this->allIpAddresses = other.allIpAddresses;
		this->ipv4Addresses = other.ipv4Addresses;
		this->mtuSize = other.mtuSize;
		this->rxSpeed = other.rxSpeed;
		this->txSpeed = other.txSpeed;
		this->interfaceUp = other.interfaceUp;
		return *this;
	}


	NetworkInterface::NetworkInterface(StringRef device, StringRef friendly, int interfaceIndex) noexcept : deviceName(device), friendlyName( friendly ), index(interfaceIndex)  {
		this->mtuSize = -1;
		this->interfaceUp = false;
		this->rxSpeed = -1;
		this->txSpeed = -1;
	}

	NetworkInterface::NetworkInterface() noexcept {
		this->mtuSize = -1;
		this->interfaceUp = false;
		this->rxSpeed = -1;
		this->txSpeed = -1;
	}


	NetworkInterface::~NetworkInterface() {

	}

	bool NetworkInterface::operator== (const NetworkInterface& other) const noexcept { return other.deviceName == deviceName; }
	bool NetworkInterface::operator!= (const NetworkInterface& other) const noexcept { return !operator== (other); }

	String NetworkInterface::toString() const {
		if (deviceName == friendlyName)
			return deviceName;
		else
			return friendlyName + " (" + deviceName + ")";
	}

	// getters
	String NetworkInterface::getFriendlyName() const {
		return friendlyName;
	}

	String NetworkInterface::getDeviceName() const {
		return deviceName;

	}

	MACAddress NetworkInterface::getMACAddress() const {
		return macAddress;
	}

	int NetworkInterface::getInterfaceIndex() {
		return index;
	}

	int NetworkInterface::getMtuSize() {
		return mtuSize;
	}

	bool NetworkInterface::isUp() {
		return interfaceUp;
	}

	int64 NetworkInterface::getRxSpeed() {
		return rxSpeed;
	}

	int64 NetworkInterface::getTxSpeed() {
		return txSpeed;
	}

	Array<IPAddress> NetworkInterface::getIPAddresses(bool includeIPv6) const {
		if (includeIPv6)
			return allIpAddresses;
		else
			return ipv4Addresses;
	}

	// setters
	void NetworkInterface::setMACAddress(MACAddress &mac) {
		macAddress = mac;
	}

	void NetworkInterface::setMtuSize(int mtu) {
		mtuSize = mtu;
	}

	void NetworkInterface::setInterfaceUp(bool isUp) {
		interfaceUp = isUp;
	}

	void NetworkInterface::setRxSpeed(int64 bps) {
		rxSpeed = bps;
	}

	void NetworkInterface::setTxSpeed(int64 bps) {
		txSpeed = bps;
	}

	void NetworkInterface::addIPAddress(IPAddress &addr) {
		if (!addr.isIPv6) {
			ipv4Addresses.addIfNotAlreadyThere(addr);
		}
		allIpAddresses.addIfNotAlreadyThere(addr);
	}

	void NetworkInterface::addIPAddresses(Array<IPAddress> &addrs) {
		for (int i = 0; i < addrs.size(); i++) {
			IPAddress adr = addrs[i];
			addIPAddress( adr );
		}
	}

	Array<NetworkInterface> NetworkInterface::getAllInterfaces()
	{
		Array<NetworkInterface> interfaces;
		findAllInterfaces(interfaces);
		return interfaces;
	}

} // namespace juce
