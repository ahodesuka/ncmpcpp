/***************************************************************************
 *   Copyright (C) 2008-2009 by Andrzej Rybczak                            *
 *   electricityispower@gmail.com                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include <algorithm>

#include "charset.h"
#include "display.h"
#include "global.h"
#include "helpers.h"
#include "playlist.h"
#include "playlist_editor.h"
#include "mpdpp.h"
#include "status.h"
#include "tag_editor.h"

using namespace Global;
using namespace MPD;
using std::string;

//Window *Global::wPlaylistEditorActiveCol;
//Menu<string> *Global::List;
//Menu<Song> *Global::Content;

PlaylistEditor *myPlaylistEditor = new PlaylistEditor;

size_t PlaylistEditor::LeftColumnWidth;
size_t PlaylistEditor::RightColumnStartX;
size_t PlaylistEditor::RightColumnWidth;

void PlaylistEditor::Init()
{
	LeftColumnWidth = COLS/3-1;
	RightColumnStartX = LeftColumnWidth+1;
	RightColumnWidth = COLS-LeftColumnWidth-1;
	
	List = new Menu<string>(0, main_start_y, LeftColumnWidth, main_height, "Playlists", Config.main_color, brNone);
	List->HighlightColor(Config.active_column_color);
	List->SetTimeout(ncmpcpp_window_timeout);
	List->SetItemDisplayer(Display::Generic);
	
	Content = new Menu<Song>(RightColumnStartX, main_start_y, RightColumnWidth, main_height, "Playlist's content", Config.main_color, brNone);
	Content->HighlightColor(Config.main_highlight_color);
	Content->SetTimeout(ncmpcpp_window_timeout);
	Content->SetSelectPrefix(&Config.selected_item_prefix);
	Content->SetSelectSuffix(&Config.selected_item_suffix);
	Content->SetItemDisplayer(Display::Songs);
	Content->SetItemDisplayerUserData(&Config.song_list_format);
	
	w = List;
}

void PlaylistEditor::Resize()
{
	LeftColumnWidth = COLS/3-1;
	RightColumnStartX = LeftColumnWidth+1;
	RightColumnWidth = COLS-LeftColumnWidth-1;
	
	List->Resize(LeftColumnWidth, main_height);
	Content->Resize(RightColumnWidth, main_height);
	
	Content->MoveTo(RightColumnStartX, main_start_y);
}

std::string PlaylistEditor::Title()
{
	return "Playlist editor";
}

void PlaylistEditor::Refresh()
{
	List->Display();
	mvvline(main_start_y, RightColumnStartX-1, 0, main_height);
	Content->Display();
}

void PlaylistEditor::SwitchTo()
{
	if (myScreen == this)
		return;
	
	CLEAR_FIND_HISTORY;
	myScreen = this;
	myPlaylist->Main()->Hide(); // hack, should be myScreen, but it doesn't always have 100% width
	redraw_header = 1;
	Refresh();
	UpdateSongList(Content);
}

void PlaylistEditor::Update()
{
	if (List->Empty())
	{
		Content->Clear(0);
		TagList list;
		Mpd->GetPlaylists(list);
		sort(list.begin(), list.end(), CaseInsensitiveSorting());
		for (TagList::iterator it = list.begin(); it != list.end(); it++)
		{
			utf_to_locale(*it);
			List->AddOption(*it);
		}
		List->Window::Clear();
		List->Refresh();
	}
	
	if (!List->Empty() && Content->Empty())
	{
		Content->Reset();
		SongList list;
		Mpd->GetPlaylistContent(locale_to_utf_cpy(List->Current()), list);
		if (!list.empty())
			Content->SetTitle("Playlist's content (" + IntoStr(list.size()) + " item" + (list.size() == 1 ? ")" : "s)"));
		else
			Content->SetTitle("Playlist's content");
		bool bold = 0;
		for (SongList::const_iterator it = list.begin(); it != list.end(); it++)
		{
			for (size_t j = 0; j < myPlaylist->Main()->Size(); j++)
			{
				if ((*it)->GetHash() == myPlaylist->Main()->at(j).GetHash())
				{
					bold = 1;
					break;
				}
			}
			Content->AddOption(**it, bold);
			bold = 0;
		}
		FreeSongList(list);
		Content->Window::Clear();
		Content->Display();
	}
	
	if (w == Content && Content->Empty())
	{
		Content->HighlightColor(Config.main_highlight_color);
		List->HighlightColor(Config.active_column_color);
		w = List;
	}
	
	if (Content->Empty())
	{
		Content->WriteXY(0, 0, 0, "Playlist is empty.");
		Content->Refresh();
	}
}

void PlaylistEditor::NextColumn()
{
	if (w == List)
	{
		CLEAR_FIND_HISTORY;
		List->HighlightColor(Config.main_highlight_color);
		w->Refresh();
		w = Content;
		Content->HighlightColor(Config.active_column_color);
	}
}

void PlaylistEditor::PrevColumn()
{
	if (w == Content)
	{
		CLEAR_FIND_HISTORY;
		Content->HighlightColor(Config.main_highlight_color);
		w->Refresh();
		w = List;
		List->HighlightColor(Config.active_column_color);
	}
}

void PlaylistEditor::AddToPlaylist(bool add_n_play)
{
	SongList list;
	
	if (w == List && !List->Empty())
	{
		Mpd->GetPlaylistContent(locale_to_utf_cpy(List->Current()), list);
		for (SongList::const_iterator it = list.begin(); it != list.end(); it++)
			Mpd->QueueAddSong(**it);
		if (Mpd->CommitQueue())
		{
			ShowMessage("Loading playlist %s...", List->Current().c_str());
			Song &s = myPlaylist->Main()->at(myPlaylist->Main()->Size()-list.size());
			if (s.GetHash() == list[0]->GetHash())
			{
				if (add_n_play)
					Mpd->PlayID(s.GetID());
			}
			else
				ShowMessage("%s", message_part_of_songs_added);
		}
	}
	else if (w == Content)
	{
		if (!Content->Empty())
		{
			block_item_list_update = 1;
			if (Config.ncmpc_like_songs_adding && Content->isBold())
			{
				long long hash = Content->Current().GetHash();
				if (add_n_play)
				{
					for (size_t i = 0; i < myPlaylist->Main()->Size(); i++)
					{
						if (myPlaylist->Main()->at(i).GetHash() == hash)
						{
							Mpd->Play(i);
							break;
						}
					}
				}
				else
				{
					Playlist::BlockUpdate = 1;
					for (size_t i = 0; i < myPlaylist->Main()->Size(); i++)
					{
						if (myPlaylist->Main()->at(i).GetHash() == hash)
						{
							Mpd->QueueDeleteSong(i);
							myPlaylist->Main()->DeleteOption(i);
							i--;
						}
					}
					Mpd->CommitQueue();
					Content->BoldOption(Content->Choice(), 0);
				}
			}
			else
			{
				const Song &s = Content->Current();
				int id = Mpd->AddSong(s);
				if (id >= 0)
				{
					ShowMessage("Added to playlist: %s", s.toString(Config.song_status_format).c_str());
					if (add_n_play)
						Mpd->PlayID(id);
					Content->BoldOption(Content->Choice(), 1);
				}
			}
		}
	}
	FreeSongList(list);
	if (!add_n_play)
		w->Scroll(wDown);
}

void PlaylistEditor::SpacePressed()
{
	if (Config.space_selects && w == Content)
	{
		Select(Content);
		w->Scroll(wDown);
		return;
	}
	AddToPlaylist(0);
}

MPD::Song *PlaylistEditor::CurrentSong()
{
	return w == Content && !Content->Empty() ? &Content->Current() : 0;
}
