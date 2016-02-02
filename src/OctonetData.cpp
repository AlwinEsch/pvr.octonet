/*
 * Copyright (C) 2015 Julian Scheel <julian@jusst.de>
 * Copyright (C) 2015 jusst technologies GmbH
 * Copyright (C) 2015 Digital Devices GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 */

#include <sstream>
#include <string>

#include <json/json.h>

#include "OctonetData.h"
#include "platform/util/StringUtils.h"

using namespace ADDON;

OctonetData::OctonetData()
{
	serverAddress = octonetAddress;
	channels.clear();
	groups.clear();

	if (loadChannelList())
		kodi->QueueNotification(QUEUE_INFO, "%d channels loaded.", channels.size());
}

OctonetData::~OctonetData(void)
{
	channels.clear();
	groups.clear();
}

bool OctonetData::loadChannelList()
{
	std::string jsonContent;
	void *f = kodi->OpenFile(("http://" + serverAddress + "/channellist.lua?select=json").c_str(), 0);
	if (!f)
		return false;

	char buf[1024];
	while (int read = kodi->ReadFile(f, buf, 1024))
		jsonContent.append(buf, read);

	kodi->CloseFile(f);

	Json::Value root;
	Json::Reader reader;

	if (!reader.parse(jsonContent, root, false))
		return false;

	const Json::Value groupList = root["GroupList"];
	for (unsigned int i = 0; i < groupList.size(); i++) {
		const Json::Value channelList = groupList[i]["ChannelList"];
		OctonetGroup group;

		group.name = groupList[i]["Title"].asString();
		group.radio = group.name.compare(0, 5, "Radio") ? false : true;

		for (unsigned int j = 0; j < channelList.size(); j++) {
			const Json::Value channel = channelList[j];
			OctonetChannel chan;

			chan.name = channel["Title"].asString();
			chan.url = "rtsp://" + serverAddress + "/" + channel["Request"].asString();
			chan.radio = group.radio;

#if 0 /* Would require a 64 bit identifier */
			std::string id = channel["ID"].asString();
			size_t strip;
			/* Strip colons from id */
			while ((strip = id.find(":")) != std::string::npos)
				id.erase(strip, 1);

			std::stringstream ids(id);
			ids >> chan.id;
#endif
			chan.id = 1000 + channels.size();
			group.members.push_back(channels.size());
			channels.push_back(chan);
		}
		groups.push_back(group);
	}

	return true;
}

void *OctonetData::Process(void)
{
	return NULL;
}

int OctonetData::getChannelCount(void)
{
	return channels.size();
}

PVR_ERROR OctonetData::getChannels(ADDON_HANDLE handle, bool bRadio)
{
	for (unsigned int i = 0; i < channels.size(); i++)
	{
		OctonetChannel &channel = channels.at(i);
		if (channel.radio == bRadio)
		{
			PVR_CHANNEL chan;
			memset(&chan, 0, sizeof(PVR_CHANNEL));

			chan.iUniqueId = channel.id;
			chan.bIsRadio = channel.radio;
			chan.iChannelNumber = i;
			strncpy(chan.strChannelName, channel.name.c_str(), strlen(channel.name.c_str()));
			strncpy(chan.strStreamURL, channel.url.c_str(), strlen(channel.url.c_str()));
			strcpy(chan.strInputFormat, "video/x-mpegts");
			chan.bIsHidden = false;

			pvr->TransferChannelEntry(handle, &chan);
		}
	}
	return PVR_ERROR_NO_ERROR;
}

int OctonetData::getGroupCount(void)
{
	return groups.size();
}

PVR_ERROR OctonetData::getGroups(ADDON_HANDLE handle, bool bRadio)
{
	for (unsigned int i = 0; i < groups.size(); i++)
	{
		OctonetGroup &group = groups.at(i);
		if (group.radio == bRadio)
		{
			PVR_CHANNEL_GROUP g;
			memset(&g, 0, sizeof(PVR_CHANNEL_GROUP));

			g.iPosition = 0;
			g.bIsRadio = group.radio;
			strncpy(g.strGroupName, group.name.c_str(), strlen(group.name.c_str()));

			pvr->TransferChannelGroup(handle, &g);
		}
	}

	return PVR_ERROR_NO_ERROR;
}

PVR_ERROR OctonetData::getGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
	OctonetGroup *g = findGroup(group.strGroupName);
	if (g == NULL)
		return PVR_ERROR_UNKNOWN;

	for (unsigned int i = 0; i < g->members.size(); i++)
	{
		OctonetChannel &channel = channels.at(g->members[i]);
		PVR_CHANNEL_GROUP_MEMBER m;
		memset(&m, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

		strncpy(m.strGroupName, group.strGroupName, strlen(group.strGroupName));
		m.iChannelUniqueId = channel.id;
		m.iChannelNumber = channel.id;

		pvr->TransferChannelGroupMember(handle, &m);
	}

	return PVR_ERROR_NO_ERROR;
}

OctonetGroup* OctonetData::findGroup(const std::string &name)
{
	for (unsigned int i = 0; i < groups.size(); i++)
	{
		if (groups.at(i).name == name)
			return &groups.at(i);
	}

	return NULL;
}
