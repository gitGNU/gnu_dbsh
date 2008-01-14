/*
    dbsh - text-based ODBC client
    Copyright (C) 2007, 2008 Ben Spencer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

wchar_t cntrl[][33] = {
	L"*NULL*",    // 0x00  (special case - NULL pointer, not NULL character)
	L"<01>",      // 0x01
	L"<02>",      // 0x02
	L"<03>",      // 0x03
	L"<04>",      // 0x04
	L"<05>",      // 0x05
	L"<06>",      // 0x06
	L"<BEL>",     // 0x07
	L"<BS>",      // 0x08
	L"        ",  // 0x09  (tab)
	L"\n",        // 0x0A  (LF)
	L"<VT>",      // 0x0B
	L"<FF>",      // 0x0C
	L"\n",        // 0x0D  (CR)
	L"<0E>",      // 0x0E
	L"<0F>",      // 0x0F
	L"<10>",      // 0x10
	L"<11>",      // 0x11
	L"<12>",      // 0x12
	L"<13>",      // 0x13
	L"<14>",      // 0x14
	L"<15>",      // 0x15
	L"<16>",      // 0x16
	L"<17" ,      // 0x17
	L"<18" ,      // 0x18
	L"<19>",      // 0x19
	L"<20>",      // 0x1A
	L"<ESC>",     // 0x1B
	L"<1C>",      // 0x1C
	L"<1D>",      // 0x1D
	L"<1E>",      // 0x1E
	L"<1F>",      // 0x1F
	L"<DEL>",     // 0x20 (hack, really 0x7F)
};
