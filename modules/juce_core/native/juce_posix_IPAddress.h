/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

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

namespace
{
    static IPAddress makeAddress (const sockaddr_in6* addr_in)
    {
        if (addr_in == nullptr)
            return {};

        auto addr = addr_in->sin6_addr;

        IPAddressByteUnion temp;
        uint16 arr[8];

        for (int i = 0; i < 8; ++i) // Swap bytes from network to host order
        {
            temp.split[0] = addr.s6_addr[i * 2 + 1];
            temp.split[1] = addr.s6_addr[i * 2];

            arr[i] = temp.combined;
        }

        return IPAddress (arr);
    }

    static IPAddress makeAddress (const sockaddr_in* addr_in)
    {
        if (addr_in->sin_addr.s_addr == INADDR_NONE)
            return {};

        return IPAddress (ntohl (addr_in->sin_addr.s_addr));
    }

    struct InterfaceInfo
    {
        IPAddress interfaceAddress, broadcastAddress;
    };

    bool operator== (const InterfaceInfo& lhs, const InterfaceInfo& rhs)
    {
        return lhs.interfaceAddress == rhs.interfaceAddress
            && lhs.broadcastAddress == rhs.broadcastAddress;
    }

    bool populateInterfaceInfo (struct ifaddrs* ifa, InterfaceInfo& interfaceInfo)
    {
        if (ifa->ifa_addr != nullptr)
        {
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                auto interfaceAddressInfo = unalignedPointerCast<sockaddr_in*> (ifa->ifa_addr);
                auto broadcastAddressInfo = unalignedPointerCast<sockaddr_in*> (ifa->ifa_dstaddr);

                if (interfaceAddressInfo->sin_addr.s_addr != INADDR_NONE)
                {
                    interfaceInfo.interfaceAddress = makeAddress (interfaceAddressInfo);
                    interfaceInfo.broadcastAddress = makeAddress (broadcastAddressInfo);
                    return true;
                }
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                interfaceInfo.interfaceAddress = makeAddress (unalignedPointerCast<sockaddr_in6*> (ifa->ifa_addr));
                interfaceInfo.broadcastAddress = makeAddress (unalignedPointerCast<sockaddr_in6*> (ifa->ifa_dstaddr));
                return true;
            }
        }

        return false;
    }

    Array<InterfaceInfo> getAllInterfaceInfo()
    {
        Array<InterfaceInfo> interfaces;
        struct ifaddrs* ifaddr = nullptr;

        if (getifaddrs (&ifaddr) != -1)
        {
            for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
            {
                InterfaceInfo i;

                if (populateInterfaceInfo (ifa, i))
                    interfaces.addIfNotAlreadyThere (i);
            }

            freeifaddrs (ifaddr);
        }

        return interfaces;
    }
}

void IPAddress::findAllAddresses (Array<IPAddress>& result, bool includeIPv6)
{
    for (auto& i : getAllInterfaceInfo())
        if (includeIPv6 || ! i.interfaceAddress.isIPv6)
            result.addIfNotAlreadyThere (i.interfaceAddress);
}

IPAddress IPAddress::getInterfaceBroadcastAddress (const IPAddress& interfaceAddress)
{
    for (auto& i : getAllInterfaceInfo())
        if (i.interfaceAddress == interfaceAddress)
            return i.broadcastAddress;

    return {};
}

int getInterfaceMtuSize( StringRef iname );
int64 getInterfaceSpeed( StringRef iname );

static bool findIndex( int idx, Array<NetworkInterface>& ifcs )
{
	if( idx < 1 )
	{
		return false;
	}
	for( int i =0; i < ifcs.size();i++ )
	{
		if( ifcs[i].getInterfaceIndex() == idx )
		{
			return true;
		}
	}
	return false;
}


void NetworkInterface::findAllInterfaces(Array<NetworkInterface>& result)
{
	const int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s != -1)
	{
		struct ifaddrs* addrs = nullptr;

		if (getifaddrs(&addrs) != -1)
		{
			for (struct ifaddrs* ifa = addrs; ifa != nullptr; ifa = ifa->ifa_next)
			{
				InterfaceInfo i;
				bool found = false;
				struct ifreq ifr;
				strcpy(ifr.ifr_name, ifa->ifa_name);
				ifr.ifr_addr.sa_family = AF_INET;
				String devName(ifa->ifa_name);
				populateInterfaceInfo(ifa, i);

				if (i.interfaceAddress.isNull())
				{
					continue;
				}

				for (int index = 0; index < result.size(); index++)
				{
					NetworkInterface ni = result[index];
					if (ni.getDeviceName() == devName)
					{
						ni.addIPAddress(i.interfaceAddress);
						result.set(index, ni);
						found = true;
						break;
					}
				}

				if (!found)
				{
					NetworkInterface ni(devName, devName, if_nametoindex(ifa->ifa_name));
					ni.addIPAddress(i.interfaceAddress);

					MACAddress ma;
					if( MACAddress::getMacAddressForInterface( devName, ma ) )
					{
						ni.setMACAddress(ma);
					}

					if (ifa->ifa_flags & IFF_UP )
					{
#if	JUCE_LINUX
						if (ifa->ifa_flags & IFF_RUNNING)
						{
#endif
							ni.setInterfaceUp(true);
#if	JUCE_LINUX
						}
#endif
					}

					int64 speed = getInterfaceSpeed( devName );
					if( speed >= 0 )
					{
						ni.setRxSpeed( speed );
						ni.setTxSpeed( speed );
					}	
					int mtu = getInterfaceMtuSize( devName );
					if( mtu >= 0 )
					{
						ni.setMtuSize( mtu );
					}

					result.add(ni);
				}
			}

			freeifaddrs(addrs);
		}

		::close(s);
	}

	/* add all non-operative interfaces */
	for (unsigned int i = 1; i; i++)
	{
		char ifname[IF_NAMESIZE + 1];
		
		if( findIndex( i, result ) )
		{
			continue;
		}
		char *name = if_indextoname(i, ifname);
		if (name)
		{
			String devName(name);
			NetworkInterface ni(devName, devName, i);
			result.add(ni);
		}
		else
		{
			break;
		}	
	}
}


} // namespace juce
