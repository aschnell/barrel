/*
 * Copyright (c) [2021-2022] SUSE LLC
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


#include "Table.h"
#include "Text.h"


namespace barrel
{

    using namespace std;


    Table::OutputInfo::OutputInfo(const Table& table)
    {
	// calculate hidden, default to false

	hidden.resize(table.header.get_columns().size());

	for (size_t i = 0; i < table.visibilities.size(); ++i)
	{
	    if (table.visibilities[i] == Visibility::AUTO || table.visibilities[i] == Visibility::OFF)
		hidden[i] = true;
	}

	for (const Table::Row& row : table.rows)
	    calculate_hidden(table, row);

	// calculate widths

	widths = table.min_widths;

	if (table.style != Style::NONE)
	    calculate_widths(table, table.header, 0);

	for (const Table::Row& row : table.rows)
	    calculate_widths(table, row, 0);
    }


    void
    Table::OutputInfo::calculate_hidden(const Table& table, const Table::Row& row)
    {
	const vector<string>& columns = row.get_columns();

	for (size_t i = 0; i < min(columns.size(), table.visibilities.size()); ++i)
	{
	    if (table.visibilities[i] == Visibility::AUTO && !columns[i].empty())
		hidden[i] = false;
	}

	for (const Table::Row& subrow : row.get_subrows())
	    calculate_hidden(table, subrow);
    }


    void
    Table::OutputInfo::calculate_widths(const Table& table, const Table::Row& row, unsigned indent)
    {
	const vector<string>& columns = row.get_columns();

	if (columns.size() > widths.size())
	    widths.resize(columns.size());

	for (size_t i = 0; i < columns.size(); ++i)
	{
	    if (hidden[i])
		continue;

	    size_t width = mbs_width(columns[i]);

	    if (i == table.tree_index)
		width += 2 * indent;

	    widths[i] = max(widths[i], width);
	}

	for (const Table::Row& subrow : row.get_subrows())
	    calculate_widths(table, subrow, indent + 1);
    }


    void
    Table::output(std::ostream& s, const Table::Row& row, const OutputInfo& output_info, const vector<bool>& lasts) const
    {
	s << string(global_indent, ' ');

	const vector<string>& columns = row.get_columns();

	for (size_t i = 0; i < output_info.widths.size(); ++i)
	{
	    if (output_info.hidden[i])
		continue;

	    string column = i < columns.size() ? columns[i] : "";

	    bool first = i == 0;
	    bool last = i == output_info.widths.size() - 1;

	    size_t extra = (i == tree_index) ? 2 * lasts.size() : 0;

	    if (last && column.empty())
		break;

	    if (!first)
		s << " ";

	    if (i == tree_index)
	    {
		for (size_t tl = 0; tl < lasts.size(); ++tl)
		{
		    if (tl == lasts.size() - 1)
			s << (lasts[tl] ? glyph(4) : glyph(3));
		    else
			s << (lasts[tl] ? glyph(6) : glyph(5));
		}
	    }

	    if (aligns[i] == Align::RIGHT)
	    {
		size_t width = mbs_width(column);

		if (width < output_info.widths[i] - extra)
		    s << string(output_info.widths[i] - width - extra, ' ');
	    }

	    s << column;

	    if (last)
		break;

	    if (aligns[i] == Align::LEFT)
	    {
		size_t width = mbs_width(column);

		if (width < output_info.widths[i] - extra)
		    s << string(output_info.widths[i] - width - extra, ' ');
	    }

	    s << " " << glyph(0);
	}

	s << '\n';

	const vector<Table::Row>& subrows = row.get_subrows();
	for (size_t i = 0; i < subrows.size(); ++i)
	{
	    vector<bool> sub_lasts = lasts;
	    sub_lasts.push_back(i == subrows.size() - 1);
	    output(s, subrows[i], output_info, sub_lasts);
	}
    }


    void
    Table::output(std::ostream& s, const OutputInfo& output_info) const
    {
	s << string(global_indent, ' ');

	for (size_t i = 0; i < output_info.widths.size(); ++i)
	{
	    if (output_info.hidden[i])
		continue;

	    for (size_t j = 0; j < output_info.widths[i]; ++j)
		s << glyph(1);

	    if (i == output_info.widths.size() - 1)
		break;

	    s << glyph(1) << glyph(2) << glyph(1);
	}

	s << '\n';
    }


    size_t
    Table::id_to_index(Id id) const
    {
	for (size_t i = 0; i < ids.size(); ++i)
	    if (ids[i] == id)
		return i;

	throw runtime_error("id not found");
    }


    string&
    Table::Row::operator[](Id id)
    {
	size_t i = table.id_to_index(id);

	if (columns.size() < i + 1)
	    columns.resize(i + 1);

	return columns[i];
    }


    Table::Table(std::initializer_list<Cell> init)
	: header(*this)
    {
	for (const Cell& cell : init)
	{
	    header.add(cell.name);
	    ids.push_back(cell.id);
	    aligns.push_back(cell.align);
	}
    }


    void
    Table::set_min_width(Id id, size_t min_width)
    {
	size_t i = id_to_index(id);

	if (min_widths.size() < i + 1)
	    min_widths.resize(i + 1);

	min_widths[i] = min_width;
    }


    void
    Table::set_visibility(Id id, Visibility visibility)
    {
	size_t i = id_to_index(id);

	if (visibilities.size() < i + 1)
	    visibilities.resize(i + 1);

	visibilities[i] = visibility;
    }


    void
    Table::set_tree_id(Id id)
    {
	tree_index = id_to_index(id);
    }


    std::ostream&
    operator<<(std::ostream& s, const Table& table)
    {
	// calculate hidden and widths

	Table::OutputInfo output_info(table);

	// output header and rows

	if (table.style != Style::NONE)
	{
	    table.output(s, table.header, output_info, {});
	    table.output(s, output_info);
	}

	for (const Table::Row& row : table.rows)
	    table.output(s, row, output_info, {});

	return s;
    }


    const char*
    Table::glyph(unsigned int i) const
    {
	const char* glyphs[][7] = {
	    { "│", "─", "┼", "├─", "└─", "│ ", "  " },  // STANDARD
	    { "║", "═", "╬", "├─", "└─", "│ ", "  " },  // DOUBLE
	    { "|", "-", "+", "+-", "+-", "| ", "  " },  // ASCII
	    { "",  "",  "",  "  ", "  ", "  ", "  " }   // NONE
	};

	return glyphs[(unsigned int)(style)][i];
    }

}
