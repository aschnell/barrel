
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE barrel

#include <boost/test/unit_test.hpp>

#include <storage/Actiongraph.h>
#include <storage/Devices/Partition.h>
#include <storage/Filesystems/Ext4.h>
#include <storage/Version.h>

#include "../barrel/handle.h"
#include "../barrel/Utils/Args.h"
#include "helpers/output.h"


using namespace std;
using namespace storage;
using namespace barrel;


BOOST_AUTO_TEST_CASE(test1)
{
    Args args({ "--dry-run", "--yes", "create", "ext4", "/dev/sdb", "-s", "8 GiB", "-p", "/test1",
	    "--mkfs-options", "-m 0", "--tune-options", "-c 0" });

    vector<string> actions = {
	"Create partition /dev/sdb1 (8.00 GiB)",
	"Create ext4 on /dev/sdb1 (8.00 GiB)",
	"Set tune options of ext4 on /dev/sdb1 (8.00 GiB)",
	"Mount /dev/sdb1 (8.00 GiB) at /test1",
	"Add mount point /test1 of /dev/sdb1 (8.00 GiB) to /etc/fstab"
    };

    Testsuite testsuite;
    testsuite.devicegraph_filename = "empty2.xml";

    handle(args.argc(), args.argv(), &testsuite);

    BOOST_CHECK_EQUAL(actions, testsuite.actions);

    const Devicegraph* staging = testsuite.storage->get_staging();

    const Partition* sdb1 = Partition::find_by_name(staging, "/dev/sdb1");
    BOOST_CHECK_EQUAL(sdb1->get_id(), ID_LINUX);

    const Ext4* ext4 = to_ext4(sdb1->get_blk_filesystem());
#if LIBSTORAGE_NG_VERSION_AT_LEAST(1, 105)
    BOOST_CHECK_EQUAL(boost::join(ext4->get_mkfs_options_v2(), " "), "-m 0");
    BOOST_CHECK_EQUAL(boost::join(ext4->get_tune_options_v2(), " "), "-c 0");
#else
    BOOST_CHECK_EQUAL(ext4->get_mkfs_options(), "-m 0");
    BOOST_CHECK_EQUAL(ext4->get_tune_options(), "-c 0");
#endif
}
