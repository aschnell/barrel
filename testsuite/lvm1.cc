
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE barrel

#include <boost/test/unit_test.hpp>

#include <storage/Actiongraph.h>

#include "../barrel/handle.h"
#include "../barrel/Utils/Args.h"


using namespace std;
using namespace storage;
using namespace barrel;


namespace std
{
    ostream& operator<<(ostream& s, const vector<string>& lines)
    {
	for (const string& line : lines)
	    s << line << '\n';

	return s;
    }
}


BOOST_AUTO_TEST_CASE(test1)
{
    Args args({ "barrel", "--dry-run", "create", "vg", "--name", "test", "--size", "5g", "--pool", "HDDs (512 B)",
	    "--devices", "2", "lv", "--name", "a", "--size", "2g", "--stripes", "max", "xfs", "--path", "/test" });

    vector<string> actions = {
	"Create partition /dev/sdb1 (2.50 GiB)",
	"Set id of partition /dev/sdb1 to Linux LVM",
	"Create physical volume on /dev/sdb1",
	"Create partition /dev/sdd1 (2.50 GiB)",
	"Set id of partition /dev/sdd1 to Linux LVM",
	"Create physical volume on /dev/sdd1",
	"Create volume group test (5.00 GiB) from /dev/sdb1 (2.50 GiB) and /dev/sdd1 (2.50 GiB)",
	"Create logical volume a (2.00 GiB) on volume group test",
	"Create xfs on /dev/test/a (2.00 GiB)",
	"Mount /dev/test/a (2.00 GiB) at /test",
	"Add mount point /test of /dev/test/a (2.00 GiB) to /etc/fstab"
    };

    Testsuite testsuite;
    testsuite.devicegraph_filename = "empty2.xml";

    vector<string> tmp;
    testsuite.save_actiongraph = [&tmp](const Actiongraph* actiongraph) {
	tmp = actiongraph->get_commit_actions_as_strings();
    };

    handle(args.argc(), args.argv(), &testsuite);

    BOOST_CHECK_EQUAL(actions, tmp); // TODO sort
}


BOOST_AUTO_TEST_CASE(test2)
{
    Args args({ "barrel", "--dry-run" });

    vector<string> actions = {
	"Create partition /dev/sdb1 (2.50 GiB)",
	"Set id of partition /dev/sdb1 to Linux LVM",
	"Create physical volume on /dev/sdb1",
	"Create partition /dev/sdd1 (2.50 GiB)",
	"Set id of partition /dev/sdd1 to Linux LVM",
	"Create physical volume on /dev/sdd1",
	"Create volume group test (5.00 GiB) from /dev/sdb1 (2.50 GiB) and /dev/sdd1 (2.50 GiB)",
	"Create logical volume a (2.00 GiB) on volume group test",
	"Create xfs on /dev/test/a (2.00 GiB)",
	"Mount /dev/test/a (2.00 GiB) at /test",
	"Add mount point /test of /dev/test/a (2.00 GiB) to /etc/fstab"
    };

    Testsuite testsuite;
    testsuite.devicegraph_filename = "empty2.xml";

    testsuite.readlines = {
	"create vg --name test --size 5g --pool 'HDDs (512 B)' --devices 2",
	"create lv --vg test --name a --size 2g --stripes 2",
	"create xfs --path /test",
	"commit"
    };

    vector<string> tmp;
    testsuite.save_actiongraph = [&tmp](const Actiongraph* actiongraph) {
	tmp = actiongraph->get_commit_actions_as_strings();
    };

    handle(args.argc(), args.argv(), &testsuite);

    BOOST_CHECK_EQUAL(actions, tmp); // TODO sort
}
