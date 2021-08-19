/*
 * Copyright (c) 2021 SUSE LLC
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
#include <storage/Storage.h>

#include "Utils/GetOpts.h"
#include "Utils/Table.h"
#include "Utils/Text.h"
#include "Utils/Misc.h"
#include "show-filesystems.h"


namespace barrel
{

    using namespace std;
    using namespace storage;


    namespace
    {

	const vector<Option> show_filesystems_options = {
	    { "probed", no_argument, 0, _("probed instead of staging") }
	};


	struct Options
	{
	    Options(GetOpts& get_opts);

	    bool show_probed = false;
	};


	Options::Options(GetOpts& get_opts)
	{
	    ParsedOpts parsed_opts = get_opts.parse("filesystems", show_filesystems_options);

	    show_probed = parsed_opts.has_option("probed");
	}

    }


    class ParsedCmdShowFilesystems : public ParsedCmd
    {
    public:

	ParsedCmdShowFilesystems(const Options& options) : options(options) {}

	virtual bool do_backup() const override { return false; }

	virtual void doit(const GlobalOptions& global_options, State& state) const override;

    private:

	const Options options;

    };


    void
    ParsedCmdShowFilesystems::doit(const GlobalOptions& global_options, State& state) const
    {
	const Storage* storage = state.storage;

	const Devicegraph* devicegraph = options.show_probed ? storage->get_probed() : storage->get_staging();

	vector<const Filesystem*> filesystems = Filesystem::get_all(devicegraph);
	// TODO sort

	Table table({ _("Type"), _("Device"), _("Mount Point") });

	for (const Filesystem* filesystem : filesystems)
	{
	    Table::Row row(table, { get_fs_type_name(filesystem->get_type()) });

	    if (is_blk_filesystem(filesystem))
	    {
		const BlkFilesystem* blk_filesystem = to_blk_filesystem(filesystem);
		row.add(blk_filesystem->get_blk_devices()[0]->get_name());
	    }
	    else
	    {
		row.add("");
	    }

	    if (filesystem->has_mount_point())
	    {
		const MountPoint* mount_point = filesystem->get_mount_point();
		row.add(mount_point->get_path());
	    }
	    else
	    {
		row.add("");
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
	return _("show filesystems");
    }


    const vector<Option>&
    CmdShowFilesystems::options() const
    {
	return show_filesystems_options;
    }

}
