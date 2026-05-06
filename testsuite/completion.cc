
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE barrel

#include <boost/test/unit_test.hpp>

#include "../barrel/Utils/CompletionHelper.h"
#include "../barrel/Utils/Args.h"

using namespace std;
using namespace barrel;


bool
item_exists(const CompletionHelper::CompletionResult& res, const string& search)
{
    auto it = find_if(res.items.begin(), res.items.end(),
	    [&search](const auto& item) {
		return item.name == search;
	    });

    return it != res.items.end();
}


bool
item_exists(const CompletionHelper::CompletionResult& res, const string& search,
	    CompletionHelper::Category category)
{
    auto it = find_if(res.items.begin(), res.items.end(),
	    [&search](const auto& item) {
		return item.name == search;
	    });

    return it != res.items.end() && it->category == category;
}


BOOST_AUTO_TEST_CASE(test_escape_unescape)
{
    BOOST_CHECK_EQUAL(CompletionHelper::escape("foo bar"), "foo\\ bar");
    BOOST_CHECK_EQUAL(CompletionHelper::unescape("foo\\ bar"), "foo bar");
    BOOST_CHECK_EQUAL(CompletionHelper::escape("no_spaces"), "no_spaces");
    BOOST_CHECK_EQUAL(CompletionHelper::unescape("no_spaces"), "no_spaces");
}


BOOST_AUTO_TEST_CASE(test_complete_commands)
{
    CompletionHelper helper;

    // Empty input, should suggest all main commands
    helper.complete({}, "");
    BOOST_CHECK(item_exists(helper.get_result(), "create"));
    BOOST_CHECK(item_exists(helper.get_result(), "show"));
    BOOST_CHECK(!item_exists(helper.get_result(), "raid"));

    // Partial input "re" -> reduce, remove, rename, resize
    helper.complete({}, "re");
    for (const auto& item : helper.get_result().items)
	BOOST_CHECK(item.name.substr(0, 2) == "re");

    helper.complete({}, "XXXXXX");
    BOOST_CHECK(helper.get_result().empty());
}


BOOST_AUTO_TEST_CASE(test_complete_subcommands)
{
    CompletionHelper helper;

    // "show " -> should suggest subcommands of show
    helper.complete({"show"}, "");
    BOOST_CHECK(item_exists(helper.get_result(), "disks"));
    BOOST_CHECK(item_exists(helper.get_result(), "pools"));
    BOOST_CHECK(!item_exists(helper.get_result(), "create"));

    // "show di" -> should suggest disks
    helper.complete({"show"}, "di");
    BOOST_CHECK(item_exists(helper.get_result(), "disks"));
    BOOST_CHECK(!item_exists(helper.get_result(), "pools"));

    // Check if we get chained subcommand
    helper.complete({"show", "disk"}, "");
    BOOST_CHECK(item_exists(helper.get_result(), "pools"));
}


BOOST_AUTO_TEST_CASE(test_complete_options)
{
    CompletionHelper helper;
    helper.complete({"show", "disks"}, "--");
    BOOST_CHECK(item_exists(helper.get_result(), "--probed"));

    // show options only once
    helper.complete({"rename", "pool", "--old-name", "HDDs (512 B)"}, "");
    BOOST_CHECK(!item_exists(helper.get_result(), "--old-name"));
    BOOST_CHECK(item_exists(helper.get_result(), "--new-name"));

    // check that show only once belongs only to the last subcommand
    helper.complete({"rename", "pool", "--old-name", "HDDs (512 B)",
	    "--new-name", "foo bar", "pool"}, "");
    BOOST_CHECK(item_exists(helper.get_result(), "--old-name"));
    BOOST_CHECK(item_exists(helper.get_result(), "--new-name"));
}


BOOST_AUTO_TEST_CASE(test_complete_option_values)
{
    CompletionHelper helper;

    helper.complete({ "create", "filesystem", "--type"}, "");
    BOOST_CHECK(item_exists(helper.get_result(), "btrfs"));
    BOOST_CHECK(item_exists(helper.get_result(), "xfs"));
    BOOST_CHECK(item_exists(helper.get_result(), "ext4"));
    BOOST_CHECK(!item_exists(helper.get_result(), "gpt"));

    // unknown option
    helper.complete({ "create", "filesystem"},  "--XXXX");
    BOOST_CHECK(helper.get_result().empty());

    // option without autocompletion
    helper.complete({ "rename", "pool", "--new-name"}, "");
    BOOST_CHECK(item_exists(helper.get_result(), "<name>"));
}


BOOST_AUTO_TEST_CASE(test_empty_results)
{
    CompletionHelper helper;

    // Unknown main command
    helper.complete({}, "nonexistent_cmd");
    BOOST_CHECK(helper.get_result().empty());

    // Unknown subcommand
    helper.complete({ "create" }, "nonexistent_subcmd");
    BOOST_CHECK(helper.get_result().empty());

    // Unknown option
    helper.complete({ "show", "disks" }, "--nonexistent_opt");
    BOOST_CHECK(helper.get_result().empty());
}


BOOST_AUTO_TEST_CASE(test_category_correctness)
{
    CompletionHelper helper;

    // Main command category
    helper.complete({}, "cr");
    BOOST_CHECK(item_exists(helper.get_result(), "create", CompletionHelper::Category::COMMAND));

    // Subcommand category
    helper.complete({ "show" }, "di");
    BOOST_CHECK(item_exists(helper.get_result(), "disks", CompletionHelper::Category::COMMAND));

    // Option category
    helper.complete({ "show", "disks" }, "--pr");
    BOOST_CHECK(item_exists(helper.get_result(), "--probed", CompletionHelper::Category::ARGUMENT));

    // Option value category (STRING_LIST)
    helper.complete({ "create", "filesystem", "--type" }, "bt");
    BOOST_CHECK(item_exists(helper.get_result(), "btrfs", CompletionHelper::Category::ARGUMENT));
}


BOOST_AUTO_TEST_CASE(test_complete_obsolete_options)
{
    CompletionHelper helper;

    // "create filesystem --" -> should not suggest obsolete --no-fstab
    helper.complete({ "create", "filesystem" }, "--");
    BOOST_CHECK(!item_exists(helper.get_result(), "--no-fstab"));
    BOOST_CHECK(item_exists(helper.get_result(), "--no-etc-fstab"));

    // "create encryption --" -> should not suggest obsolete --no-crypttab
    helper.complete({ "create", "encryption" }, "--");
    BOOST_CHECK(!item_exists(helper.get_result(), "--no-crypttab"));
    BOOST_CHECK(item_exists(helper.get_result(), "--no-etc-crypttab"));
}


BOOST_AUTO_TEST_CASE(test_storage_dependent_completions)
{
    Testsuite testsuite;
    testsuite.devicegraph_filename = "real1.xml";

    Args args({ "--dry-run", "help" }); // Dummy call to initialize everything
    handle(args.argc(), args.argv(), &testsuite);

    CompletionHelper helper;
    helper.set_storage(testsuite.storage.get());

    // "show tree /dev/s" -> should suggest /dev/sda, /dev/sdb, /dev/sdc etc from real1.xml
    helper.complete({"show", "tree"}, "/dev/s");
    BOOST_CHECK(item_exists(helper.get_result(), "/dev/sda", CompletionHelper::Category::DEVICE));

    helper.complete({ "create", "lv", "--vg-name" }, "da");
    BOOST_CHECK(item_exists(helper.get_result(), "data", CompletionHelper::Category::LVM_VG));
}


BOOST_AUTO_TEST_CASE(test_pool_completions)
{
    Testsuite testsuite;
    testsuite.devicegraph_filename = "real1.xml";

    Args args({ "--dry-run", "help" });
    handle(args.argc(), args.argv(), &testsuite);

    testsuite.storage->create_pool("swimming-pool");

    CompletionHelper helper;
    helper.set_storage(testsuite.storage.get());

    // "create filesystem --pool-name swim" -> must suggest swimming-pool
    helper.complete({ "create", "filesystem", "--pool-name" }, "swim");
    BOOST_CHECK(item_exists(helper.get_result(), "swimming-pool", CompletionHelper::Category::POOL));
}


BOOST_AUTO_TEST_CASE(test_path_completions)
{
    CompletionHelper helper;

    // Path completion for --path. Since it uses list_files_of_dir, we can't easily
    // mock the filesystem, but /dev should always exist.
    helper.complete({ "create", "filesystem", "--path" }, "/");
    BOOST_CHECK(item_exists(helper.get_result(), "/dev/", CompletionHelper::Category::DEVICE));
}


BOOST_AUTO_TEST_CASE(test_short_options_values)
{
    CompletionHelper helper;

    // "-t" for create filesystem should suggest types.
    helper.complete({ "create", "filesystem", "-t" }, "");
    BOOST_CHECK(item_exists(helper.get_result(), "btrfs", CompletionHelper::Category::ARGUMENT));

    // But it must not suggest short options themselves when completing "--".
    helper.complete({ "create", "filesystem" }, "--");
    for (const auto& item : helper.get_result().items)
	BOOST_CHECK_EQUAL(item.name.substr(0, 2), "--");
}
