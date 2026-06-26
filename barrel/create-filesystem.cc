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


#include <boost/algorithm/string.hpp>

#include <storage/Storage.h>
#include <storage/Pool.h>
#include <storage/Devices/BlkDevice.h>
#include <storage/Devices/PartitionTable.h>
#include <storage/Devices/Partitionable.h>
#include <storage/Devices/Partition.h>
#include <storage/Filesystems/MountPoint.h>
#include <storage/Filesystems/Btrfs.h>
#include <storage/SystemInfo/SystemInfo.h>
#include <storage/Utils/HumanString.h>
#include <storage/Version.h>

#include "Utils/GetOpts.h"
#include "Utils/Text.h"
#include "Utils/Misc.h"
#include "Utils/BarrelTmpl.h"
#include "create-filesystem.h"


namespace barrel
{

    using namespace storage;


    namespace
    {


	const map<string, FsType> str_to_fs_type = {
#if LIBSTORAGE_NG_VERSION_AT_LEAST(1, 100)
	    { "bcachefs", FsType::BCACHEFS },
#endif
	    { "btrfs", FsType::BTRFS },
	    { "exfat", FsType::EXFAT },
	    { "ext2", FsType::EXT2 },
	    { "ext3", FsType::EXT3 },
	    { "ext4", FsType::EXT4 },
	    { "f2fs", FsType::F2FS },
	    { "jfs", FsType::JFS },
	    { "nilfs2", FsType::NILFS2 },
	    { "ntfs", FsType::NTFS },
	    { "reiserfs", FsType::REISERFS },
	    { "swap", FsType::SWAP },
	    { "udf", FsType::UDF },
	    { "vfat", FsType::VFAT },
	    { "xfs", FsType::XFS }
	};


	const map<string, MountByType> str_to_mount_by_type = {
	    { "device", MountByType::DEVICE },
	    { "path", MountByType::PATH },
	    { "id", MountByType::ID },
	    { "uuid", MountByType::UUID },
	    { "label", MountByType::LABEL },
#if 0
	    { "partuuid", MountByType::PARTUUID },
	    { "partlabel", MountByType::PARTLABEL }
#endif
	};


	const map<string, BtrfsRaidLevel> str_to_btrfs_raid_level = {
	    { "default", BtrfsRaidLevel::DEFAULT },
	    { "single", BtrfsRaidLevel::SINGLE },
	    { "dup", BtrfsRaidLevel::DUP },
	    { "raid0", BtrfsRaidLevel::RAID0 },
	    { "raid1", BtrfsRaidLevel::RAID1 },
	    { "raid1c3", BtrfsRaidLevel::RAID1C3 },
	    { "raid1c4", BtrfsRaidLevel::RAID1C4 },
	    { "raid5", BtrfsRaidLevel::RAID5 },
	    { "raid6", BtrfsRaidLevel::RAID6 },
	    { "raid10", BtrfsRaidLevel::RAID10 }
	};


	const ExtOptions create_filesystem_options({
	    { "type", required_argument, 't', _("set file system type"), "type",
		ValueType::STRING_LIST, map_keys(str_to_fs_type) },
	    { "label", required_argument, 'l', _("set file system label"), "label" },
	    { "path", required_argument, 'p', _("mount path"), "path", ValueType::PATH },
	    { "mount-by", required_argument, 0, _("mount by"), "type",
		ValueType::STRING_LIST, map_keys(str_to_mount_by_type) },
	    { "mount-options", required_argument, 'o', _("mount options"), "options" },
	    { "mkfs-options", required_argument, 0, _("mkfs options"), "options" },
	    { "tune-options", required_argument, 0, _("tune options"), "options" },
	    { "pool-name", required_argument, 0, _("pool name"), "name", ValueType::POOL },
	    { "size", required_argument, 's', _("set size"), "size" },
	    { "devices", required_argument, 'd', _("set number of devices"), "number" },
	    { "profiles", required_argument, 0, _("set profiles"), "profiles",
		ValueType::STRING_LIST, map_keys(str_to_btrfs_raid_level) },
	    { "no-etc-fstab", no_argument, 0, _("do not add in /etc/fstab") },
	    { "no-fstab", no_argument, 0, nullptr }, // deprecated
	    { "force", no_argument, 0, _("force if block devices are in use") }
	}, TakeBlkDevices::MAYBE);


	BtrfsRaidLevel
	parse_btrfs_raid_level(const string& str)
	{
	    map<string, BtrfsRaidLevel>::const_iterator it = str_to_btrfs_raid_level.find(str);
	    if (it == str_to_btrfs_raid_level.end())
		throw runtime_error(sformat(_("unknown btrfs profile '%s'"), str.c_str()));

	    return it->second;
	}


	bool
	supports_multiple_devices(FsType fs_type)
	{
	    return fs_type == FsType::BTRFS;
	}


	struct Options
	{
	    Options(GetOpts& get_opts);

	    optional<FsType> type;
	    optional<string> label;
	    optional<string> path;
	    optional<MountByType> mount_by;
	    optional<vector<string>> mount_options;
	    optional<string> mkfs_options;
	    optional<string> tune_options;
	    optional<string> pool_name;
	    optional<SmartSize> size;
	    optional<SmartNumber> number;
	    BtrfsRaidLevel btrfs_data_raid_level = BtrfsRaidLevel::DEFAULT;
	    BtrfsRaidLevel btrfs_metadata_raid_level = BtrfsRaidLevel::DEFAULT;
	    bool etc_fstab = true;
	    bool force = false;

	    vector<string> blk_devices;

	    enum class ModusOperandi { BLK_DEVICES, POOL, PARTITION_TABLES_FROM_STACK, BLK_DEVICES_FROM_STACK,
		PARTITIONABLES };

	    ModusOperandi modus_operandi;

	    void calculate_modus_operandi();

	    void check() const;
	};


	Options::Options(GetOpts& get_opts)
	{
	    ParsedOpts parsed_opts = get_opts.parse("filesystem", create_filesystem_options);

	    if (parsed_opts.has_option("type"))
	    {
		string str = parsed_opts.get("type");

		map<string, FsType>::const_iterator it = str_to_fs_type.find(str);
		if (it == str_to_fs_type.end())
		    throw runtime_error(sformat(_("unknown filesystem type '%s'"), str.c_str()));

		type = it->second;
	    }

	    label = parsed_opts.get_optional("label");

	    path = parsed_opts.get_optional("path");

	    if (parsed_opts.has_option("mount-by"))
	    {
		string str = parsed_opts.get("mount-by");

		map<string, MountByType>::const_iterator it = str_to_mount_by_type.find(str);
		if (it == str_to_mount_by_type.end())
		    throw runtime_error(sformat(_("unknown mount-by type '%s'"), str.c_str()));

		mount_by = it->second;
	    }

	    if (parsed_opts.has_option("mount-options"))
	    {
		string str = parsed_opts.get("mount-options");

		vector<string> tmp;
		boost::split(tmp, str, boost::is_any_of(","), boost::token_compress_on);
		mount_options = tmp;
	    }

	    if (parsed_opts.has_option("mkfs-options"))
	    {
		mkfs_options = parsed_opts.get("mkfs-options");
		parse_line(mkfs_options.value()); // just a sanity check
	    }

	    if (parsed_opts.has_option("tune-options"))
	    {
		tune_options = parsed_opts.get("tune-options");
		parse_line(tune_options.value()); // just a sanity check
	    }

	    pool_name = parsed_opts.get_optional("pool-name");

	    if (parsed_opts.has_option("size"))
	    {
		string str = parsed_opts.get("size");
		size = SmartSize(str);

		// The min_size() function of libstorage-ng is not available here.
		if (size.value().type == SmartSize::ABSOLUTE && size.value().value < 64 * KiB)
		    throw runtime_error(_("size too small for file system"));
	    }

	    if (parsed_opts.has_option("devices"))
	    {
		string str = parsed_opts.get("devices");
		number = SmartNumber(str);
	    }

	    if (parsed_opts.has_option("profiles"))
	    {
		string str = parsed_opts.get("profiles");

		string::size_type pos = str.find(',');
		if (pos == string::npos)
		{
		    btrfs_data_raid_level = btrfs_metadata_raid_level = parse_btrfs_raid_level(str);
		}
		else
		{
		    btrfs_data_raid_level = parse_btrfs_raid_level(str.substr(0, pos));
		    btrfs_metadata_raid_level = parse_btrfs_raid_level(str.substr(pos + 1));
		}
	    }

	    etc_fstab = !parsed_opts.has_option("no-etc-fstab") && !parsed_opts.has_option("no-fstab");

	    force = parsed_opts.has_option("force");

	    blk_devices = parsed_opts.get_blk_devices();

	    calculate_modus_operandi();
	}


	void
	Options::calculate_modus_operandi()
	{
	    // TODO identical in create-lvm-vg.cc

	    if (pool_name)
	    {
		if (!size)
		    throw runtime_error(_("size argument missing for command 'filesystem'"));

		if (!blk_devices.empty())
		    throw runtime_error(_("pool argument and blk devices not allowed together for command 'filesystem'"));

		modus_operandi = ModusOperandi::POOL;
	    }
	    else
	    {
		if (size)
		{
		    if (blk_devices.empty())
			modus_operandi = ModusOperandi::PARTITION_TABLES_FROM_STACK;
		    else
			modus_operandi = ModusOperandi::PARTITIONABLES;
		}
		else
		{
		    if (blk_devices.empty())
			modus_operandi = ModusOperandi::BLK_DEVICES_FROM_STACK;
		    else
			modus_operandi = ModusOperandi::BLK_DEVICES;
		}
	    }
	}


	void
	Options::check() const
	{
	    if (!type)
	    {
		throw logic_error("filesystem type still unknown");
	    }

	    if (tune_options)
	    {
		if (type.value() != FsType::EXT2 && type.value() != FsType::EXT3 &&
		    type.value() != FsType::EXT4)
		    throw runtime_error(sformat(_("tune options not allowed for %s"),
						get_fs_type_name(type.value()).c_str()));
	    }

	    if (path)
	    {
		const FsType fs_type = type.value();

		if (!Mountable::is_valid_path(fs_type, path.value()))
		{
		    switch (fs_type)
		    {
			case FsType::SWAP:
			    // TRANSLATORS: do not translate 'swap' nor 'none'
			    throw runtime_error(_("path must be 'swap' or 'none'"));

			default:
			    throw runtime_error(sformat(_("invalid path '%s'"), path.value().c_str()));
		    }
		}
	    }
	    else
	    {
		if (mount_by)
		    throw runtime_error(_("mount-by requires a path"));

		if (mount_options)
		    throw runtime_error(_("mount options require a path"));
	    }

	    if (mount_by && mount_by.value() == MountByType::LABEL && !label)
	    {
		throw runtime_error(_("mount-by label requires a label"));
	    }

	    if (number && !supports_multiple_devices(type.value()))
	    {
		throw runtime_error(sformat(_("option --devices not allowed for %s"),
					    get_fs_type_name(type.value()).c_str()));
	    }
	}

    }


    class ParsedCmdCreateFilesystem : public ParsedCmd
    {
    public:

	ParsedCmdCreateFilesystem(const Options& options)
	    : options(options)
	{
	    options.check();
	}

	virtual bool do_backup() const override { return true; }

	virtual void doit(const GlobalOptions& global_options, State& state) const override;

    private:

	const Options options;

    };


    void
    ParsedCmdCreateFilesystem::doit(const GlobalOptions& global_options, State& state) const
    {
	Devicegraph* staging = state.storage->get_staging();

	FsType fs_type = options.type.value();

	vector<BlkDevice*> blk_devices;

	switch (options.modus_operandi)
	{
	    case Options::ModusOperandi::BLK_DEVICES_FROM_STACK:
	    {
		blk_devices = state.stack.top_as_blk_devices(staging);

		if (!supports_multiple_devices(fs_type) && blk_devices.size() > 1)
		    throw runtime_error(_("only one block device allowed"));

		state.stack.pop();
	    }
	    break;

	    case Options::ModusOperandi::POOL:
	    {
		Pool* pool = state.storage->get_pool(options.pool_name.value());

		blk_devices = PartitionCreator::create_partitions(pool, staging, PartitionCreator::ONE,
								  options.number, options.size.value());
	    }
	    break;

	    case Options::ModusOperandi::PARTITION_TABLES_FROM_STACK:
	    {
		vector<PartitionTable*> partition_tables = state.stack.top_as_partition_tables(staging);

		if (!supports_multiple_devices(fs_type) && partition_tables.size() > 1)
		    throw runtime_error(_("only one partition table allowed"));

		Pool pool;

		for (const PartitionTable* partition_table : partition_tables)
		    pool.add_device(partition_table->get_partitionable());

		blk_devices = PartitionCreator::create_partitions(&pool, staging, PartitionCreator::POOL_SIZE,
								  options.number, options.size.value());

		state.stack.pop();
	    }
	    break;

	    case Options::ModusOperandi::BLK_DEVICES:
	    {
		if (options.blk_devices.size() < 1)
		    throw runtime_error(_("missing block device"));

		if (!supports_multiple_devices(fs_type) && options.blk_devices.size() > 1)
		    throw runtime_error(_("only one block device allowed"));

		for (const string& device_name : options.blk_devices)
		{
		    blk_devices.push_back(BlkDevice::find_by_name(staging, device_name));
		}
	    }
	    break;

	    case Options::ModusOperandi::PARTITIONABLES:
	    {
		if (options.blk_devices.size() < 1)
		    throw runtime_error(_("missing partitionable"));

		if (!supports_multiple_devices(fs_type) && options.blk_devices.size() > 1)
		    throw runtime_error(_("only one partitionable allowed"));

		Pool pool;

		for (const string& device_name : options.blk_devices)
		{
		    const Partitionable* partitionable = Partitionable::find_by_name(staging, device_name);
		    pool.add_device(partitionable);
		}

		blk_devices = PartitionCreator::create_partitions(&pool, staging, PartitionCreator::POOL_SIZE,
								  options.number, options.size.value());
	    }
	    break;
	}

	if (blk_devices.empty())
	    throw runtime_error(_("block devices for filesystem missing"));

	check_usable(blk_devices, options.force);

	if (fs_type != FsType::SWAP && options.path)
	{
	    string path = options.path.value();

	    if (!MountPoint::find_by_path(staging, path).empty())
		throw runtime_error(sformat(_("path '%s' already used"), path.c_str()));
	}

	BlkFilesystem* blk_filesystem = blk_devices[0]->create_blk_filesystem(fs_type);

	if (fs_type == FsType::BTRFS)
	{
	    Btrfs* btrfs = to_btrfs(blk_filesystem);

	    for (vector<BlkDevice*>::iterator it = blk_devices.begin(); it != blk_devices.end(); ++it)
	    {
		if (it != blk_devices.begin())
		    btrfs->add_device(*it);
	    }

	    if (options.btrfs_data_raid_level != BtrfsRaidLevel::DEFAULT &&
		!contains(btrfs->get_allowed_data_raid_levels(), options.btrfs_data_raid_level))
		throw runtime_error("unsupported number of devices for profile");
	    btrfs->set_data_raid_level(options.btrfs_data_raid_level);

	    if (options.btrfs_metadata_raid_level != BtrfsRaidLevel::DEFAULT &&
		!contains(btrfs->get_allowed_metadata_raid_levels(), options.btrfs_metadata_raid_level))
		throw runtime_error("unsupported number of devices for profile");
	    btrfs->set_metadata_raid_level(options.btrfs_metadata_raid_level);
	}

	if (options.label)
	{
	    blk_filesystem->set_label(options.label.value());
	}

	if (options.mkfs_options)
	{
	    blk_filesystem->set_mkfs_options(options.mkfs_options.value());
	}

	if (options.tune_options)
	{
	    blk_filesystem->set_tune_options(options.tune_options.value());
	}

	if (options.path)
	{
	    string path = options.path.value();
	    MountPoint* mount_point = blk_filesystem->create_mount_point(path);

	    mount_point->set_in_etc_fstab(options.etc_fstab);

	    if (options.mount_by)
		mount_point->set_mount_by(options.mount_by.value());

	    if (options.mount_options)
		mount_point->set_mount_options(options.mount_options.value());
	}

	{
	    unsigned int id = ID_LINUX;

	    if (fs_type == FsType::SWAP)
	    {
		id = ID_SWAP;
	    }
	    else if (options.path)
	    {
		SystemInfo system_info;

		const string& path = options.path.value();

		if (path == "/")
		    id = get_linux_partition_id(LinuxPartitionIdCategory::ROOT, system_info);
		else if (path == "/usr")
		    id = get_linux_partition_id(LinuxPartitionIdCategory::USR, system_info);
		else if (path == "/home")
		    id = ID_LINUX_HOME;
		else if (path == "/srv")
		    id = ID_LINUX_SERVER_DATA;
	    }

	    for (BlkDevice* blk_device : blk_devices)
	    {
		if (is_partition(blk_device))
		{
		    Partition* partition = to_partition(blk_device);
		    const PartitionTable* partition_table = partition->get_partition_table();

		    if (partition_table->is_partition_id_supported(id))
			partition->set_id(id);
		    else
			partition->set_id(ID_LINUX);
		}
	    }
	}

	state.stack.push(blk_filesystem);
	state.modified = true;
    }


    shared_ptr<ParsedCmd>
    parse_create_filesystem(GetOpts& get_opts, FsType type)
    {
	Options options(get_opts);

	if (options.type)
	    throw runtime_error(_("filesystem type already set for command 'filesystem'"));

	options.type = type;

	return make_shared<ParsedCmdCreateFilesystem>(options);
    }


    shared_ptr<ParsedCmd>
    CmdCreateFilesystem::parse(GetOpts& get_opts) const
    {
	Options options(get_opts);

	if (!options.type)
	    throw runtime_error(_("filesystem type missing for command 'filesystem'"));

	return make_shared<ParsedCmdCreateFilesystem>(options);
    }


    const char*
    CmdCreateFilesystem::help() const
    {
	return _("Creates a new file system.");
    }


    const ExtOptions&
    CmdCreateFilesystem::options() const
    {
	return create_filesystem_options;
    }


#if LIBSTORAGE_NG_VERSION_AT_LEAST(1, 100)

    shared_ptr<ParsedCmd>
    CmdCreateBcachefs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::BCACHEFS);
    }


    const char*
    CmdCreateBcachefs::help() const
    {
	return _("Alias for 'create filesystem --type bcachefs'");
    }

#endif


    shared_ptr<ParsedCmd>
    CmdCreateBtrfs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::BTRFS);
    }


    const char*
    CmdCreateBtrfs::help() const
    {
	return _("Alias for 'create filesystem --type btrfs'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateExfat::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::EXFAT);
    }


    const char*
    CmdCreateExfat::help() const
    {
	return _("Alias for 'create filesystem --type exfat'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateExt2::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::EXT2);
    }


    const char*
    CmdCreateExt2::help() const
    {
	return _("Alias for 'create filesystem --type ext2'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateExt3::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::EXT3);
    }


    const char*
    CmdCreateExt3::help() const
    {
	return _("Alias for 'create filesystem --type ext3'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateExt4::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::EXT4);
    }


    const char*
    CmdCreateExt4::help() const
    {
	return _("Alias for 'create filesystem --type ext4'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateF2fs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::F2FS);
    }


    const char*
    CmdCreateF2fs::help() const
    {
	return _("Alias for 'create filesystem --type f2fs'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateJfs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::JFS);
    }


    const char*
    CmdCreateJfs::help() const
    {
	return _("Alias for 'create filesystem --type jfs'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateNilfs2::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::NILFS2);
    }


    const char*
    CmdCreateNilfs2::help() const
    {
	return _("Alias for 'create filesystem --type nilfs2'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateNtfs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::NTFS);
    }


    const char*
    CmdCreateNtfs::help() const
    {
	return _("Alias for 'create filesystem --type ntfs'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateReiserfs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::REISERFS);
    }


    const char*
    CmdCreateReiserfs::help() const
    {
	return _("Alias for 'create filesystem --type reiserfs'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateSwap::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::SWAP);
    }


    const char*
    CmdCreateSwap::help() const
    {
	return _("Alias for 'create filesystem --type swap'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateUdf::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::UDF);
    }


    const char*
    CmdCreateUdf::help() const
    {
	return _("Alias for 'create filesystem --type udf'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateVfat::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::VFAT);
    }


    const char*
    CmdCreateVfat::help() const
    {
	return _("Alias for 'create filesystem --type vfat'");
    }


    shared_ptr<ParsedCmd>
    CmdCreateXfs::parse(GetOpts& get_opts) const
    {
	return parse_create_filesystem(get_opts, FsType::XFS);
    }


    const char*
    CmdCreateXfs::help() const
    {
	return _("Alias for 'create filesystem --type xfs'");
    }

}
