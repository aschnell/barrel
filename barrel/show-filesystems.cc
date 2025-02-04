/*
 * Copyright (c) [2021-2025] SUSE LLC
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact SUSE LLC.
 *
 * To contact SUSE LLC about this file by physical or electronic mail, you may
 * find current contact information at www.suse.com.
 */


#include <storage/Devices/BlkDevice.h>
#include <storage/Filesystems/Mountable.h>
#include <storage/Filesystems/MountPoint.h>
#include <storage/Filesystems/Filesystem.h>
#include <storage/Filesystems/BlkFilesystem.h>
#include <storage/Filesystems/Btrfs.h>
#include <storage/Filesystems/Swap.h>
#include <storage/Filesystems/Nfs.h>
#include <storage/Filesystems/Tmpfs.h>
#include <storage/Storage.h>
#include <storage/Environment.h>
#include <storage/FreeInfo.h>

#include "Utils/GetOpts.h"
#include "Utils/Table.h"
#include "Utils/Text.h"
#include "Utils/Misc.h"
#include "show-filesystems.h"
#include "show.h"


namespace barrel
{

    using namespace std;
    using namespace storage;


    namespace
    {

	const ExtOptions show_filesystems_options({
	    { "probed", no_argument, 0, _("use probed instead of staging devicegraph") },
	    { "probe-used-space", required_argument, 0, _("probe used space"), "mode" }
	});


	struct Options
	{
	    enum class ProbeUsedSpace { NONE, ACTIVE, ALL };

	    Options(GetOpts& get_opts);

	    bool show_probed = false;
	    ProbeUsedSpace probe_used_space = ProbeUsedSpace::ACTIVE;
	};


	const map<string, Options::ProbeUsedSpace> str_to_probe_used_space = {
	    { "none", Options::ProbeUsedSpace::NONE },
	    { "active", Options::ProbeUsedSpace::ACTIVE },
	    { "all", Options::ProbeUsedSpace::ALL }
	};


	Options::Options(GetOpts& get_opts)
	{
	    ParsedOpts parsed_opts = get_opts.parse("filesystems", show_filesystems_options);

	    show_probed = parsed_opts.has_option("probed");

	    if (parsed_opts.has_option("probe-used-space"))
	    {
		string str = parsed_opts.get("probe-used-space");

		map<string, Options::ProbeUsedSpace>::const_iterator it = str_to_probe_used_space.find(str);
		if (it == str_to_probe_used_space.end())
		    throw runtime_error(sformat(_("unknown probe-used-space mode '%s'"), str.c_str()));

		probe_used_space = it->second;
	    }
	}

    }


    class ParsedCmdShowFilesystems : public ParsedCmdShow
    {
    public:

	ParsedCmdShowFilesystems(const Options& options) : options(options) {}

	virtual bool do_backup() const override { return false; }

	virtual void doit(const GlobalOptions& global_options, State& state) const override;

    private:

	const Options options;

	static bool compare_by_something(const Filesystem* lhs, const Filesystem* rhs);

	void probe_used_space(const GlobalOptions& global_options, const State& state) const;

    };


    bool
    ParsedCmdShowFilesystems::compare_by_something(const Filesystem* lhs, const Filesystem* rhs)
    {
	bool t1 = lhs->has_mount_point();
	bool t2 = rhs->has_mount_point();

	if (t1 && t2)
	    return lhs->get_mount_point()->get_path() < rhs->get_mount_point()->get_path();

	if (t1 && !t2)
	    return true;

	if (!t1 && t2)
	    return false;

	// TODO and now?

	return false;
    }


    void
    ParsedCmdShowFilesystems::probe_used_space(const GlobalOptions& global_options, const State& state) const
    {
	// TODO move functionality to libstorage-ng?

	const Storage* storage = state.storage;
	const Environment& environment = storage->get_environment();

	if (environment.get_probe_mode() != ProbeMode::STANDARD)
	    return;

	const Devicegraph* devicegraph = storage->get_probed();

	for (const Filesystem* filesystem : Filesystem::get_all(devicegraph))
	{
	    if (!is_blk_filesystem(filesystem) || is_swap(filesystem))
		continue;

	    if (options.show_probed)
	    {
		if (!filesystem->exists_in_probed())
		    continue;
	    }
	    else
	    {
		if (!filesystem->exists_in_staging())
		    continue;
	    }

	    switch (options.probe_used_space)
	    {
		case Options::ProbeUsedSpace::NONE:
		{
		    continue;
		}
		break;

		case Options::ProbeUsedSpace::ACTIVE:
		{
		    if (!filesystem->has_mount_point())
			continue;

		    const MountPoint* mount_point = filesystem->get_mount_point();
		    if (!mount_point->is_active())
			continue;
		}
		break;

		case Options::ProbeUsedSpace::ALL:
		{
		}
		break;
	    }

	    filesystem->detect_space_info();
	}
    }


    void
    ParsedCmdShowFilesystems::doit(const GlobalOptions& global_options, State& state) const
    {
	const Storage* storage = state.storage;
	const Environment& environment = storage->get_environment();
	const string& rootprefix = environment.get_rootprefix();

	const Devicegraph* devicegraph = options.show_probed ? storage->get_probed() : storage->get_staging();

	vector<const Filesystem*> filesystems = Filesystem::get_all(devicegraph);
	sort(filesystems.begin(), filesystems.end(), compare_by_something);

	probe_used_space(global_options, state);

	Table table({ _("Type"), Cell(_("Label"), Id::LABEL), Cell(_("Name"), Id::NAME),
		Cell(_("Size"), Id::SIZE, Align::RIGHT), Cell(_("Used"), Id::USED, Align::RIGHT),
		Cell(_("Profiles"), Id::PROFILES), Cell(_("Mount Point"), Id::MOUNT_POINT) });
	table.set_style(global_options.table_style);
	table.set_tree_id(Id::NAME);
	table.set_visibility(Id::USED, Visibility::AUTO);
	table.set_visibility(Id::PROFILES, Visibility::AUTO);

	for (const Filesystem* filesystem : filesystems)
	{
	    Table::Row row(table, { get_fs_type_name(filesystem->get_type()) });

	    if (is_blk_filesystem(filesystem))
	    {
		const BlkFilesystem* blk_filesystem = to_blk_filesystem(filesystem);
		row[Id::LABEL] = blk_filesystem->get_label();

		vector<const BlkDevice*> blk_devices = blk_filesystem->get_blk_devices();

		if (blk_devices.size() == 1)
		{
		    const BlkDevice* blk_device = blk_devices[0];

		    row[Id::NAME] = blk_device->get_name();
		    row[Id::SIZE] = format_size(blk_device->get_size());
		}
		else
		{
		    sort(blk_devices.begin(), blk_devices.end(), BlkDevice::compare_by_name);

		    row[Id::NAME] = _("multiple devices");

		    for (const BlkDevice* blk_device : blk_devices)
		    {
			Table::Row subrow(row.get_table());
			subrow[Id::NAME] = blk_device->get_name();
			subrow[Id::SIZE] = format_size(blk_device->get_size());
			row.add_subrow(subrow);
		    }
		}

		if (is_btrfs(blk_filesystem))
		{
		    const Btrfs* btrfs = to_btrfs(blk_filesystem);

		    BtrfsRaidLevel data_raid_level = btrfs->get_data_raid_level();
		    BtrfsRaidLevel metadata_raid_level = btrfs->get_metadata_raid_level();

		    string tmp = get_btrfs_raid_level_name(data_raid_level);
		    if (metadata_raid_level != data_raid_level)
			tmp += ", " + get_btrfs_raid_level_name(metadata_raid_level);

		    row[Id::PROFILES] = boost::to_lower_copy(tmp, locale::classic());
		}
	    }
	    else if (is_nfs(filesystem))
	    {
		const Nfs* nfs = to_nfs(filesystem);
		row[Id::NAME] = nfs->get_server() + ":" + nfs->get_path();
	    }

	    if (filesystem->has_space_info())
	    {
		SpaceInfo space_info = filesystem->detect_space_info();
		unsigned long long total_size = space_info.size;
		unsigned long long total_used = space_info.used;

		if (is_nfs(filesystem) || is_tmpfs(filesystem))
		    row[Id::SIZE] = format_size(space_info.size);

		row[Id::USED] = format_percentage(total_used, total_size, 1);
	    }

	    if (filesystem->has_mount_point())
	    {
		const MountPoint* mount_point = filesystem->get_mount_point();

		string tmp = mount_point->get_path();

		if (!rootprefix.empty())
		{
		    if (mount_point->is_rootprefixed() && !is_swap(filesystem))
			tmp = "[" + rootprefix + "] " + tmp;
		}

		if (!mount_point->is_active())
		    tmp += " !";

		row[Id::MOUNT_POINT] = tmp;
	    }

	    table.add(row);
	}

	cout << table;
    }


    shared_ptr<ParsedCmd>
    CmdShowFilesystems::parse(GetOpts& get_opts) const
    {
	Options options(get_opts);

	return make_shared<ParsedCmdShowFilesystems>(options);
    }


    const char*
    CmdShowFilesystems::help() const
    {
	return _("Shows filesystems.");
    }


    const ExtOptions&
    CmdShowFilesystems::options() const
    {
	return show_filesystems_options;
    }

}
