# Changelog

06.04.2026, Sandy Marko Knauer <sandy.marko.k@gmail.com>

Add "RequiresAdminRights" to the installation configuration template and remove it from the package template.

25.03.2026, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- fix: show preview (open file path) from file dialog

11.03.2026, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- The SQL query for Notes has been modified so that the last entry that existed
at that time is always displayed. This prevents notes from simply not being shown,
and also allows notes to be edited retroactively if they were loaded via the calendar.

06.03.2026, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- fix load note text from songId

05.03.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Notes Markdown fixes (hyperlink and more)
- Add auto save for notes (10 sec)
- Add auto save for practice table (30 sec)
- Note edit workflow improved by an Edit and Preview button

03.03.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Data import from Menu reload page fixed
- FNVH1: fixed false positiv smalls files
- Feature: Message for unsaved session
- Hash when openFile pressed, make hash update before before switch song. #12
- Fixed wrong song information - startup

02.03.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Fixed a problem with remembering when something is marked as done.
- Tooltip for reminders implemented.
- New features translated into German.
- code refactoring - unused functions removed
- Import dialog from menu fixed
- Librarypage add rename file and copy filename to clipboard
- Menu: Add Update

01.03.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Wizard toggle move files
- Fixed parse Files when move files active (path corrected)

29.02.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- fixed error when targetdir not exits.
- File extensions for documents and videos have been expanded.

28.02.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Translation corrected
- Weekly Reminder fixed
- Adopt QSpinBox training values ​​for start bar, end bar and bpm

27.06.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Add move file option
- Add Database settings is_moved and reminder is_weekly

26.02.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- DatabaseManager getPracticeSessions optimation
- LessonPage load last 3 sessions.
- Fixed ToolButton The video was linked to unlinked, which is why the list of linked files remained empty.
- Fixed ReminderDialog header missing ifndef, define and endif

25.02.26, Sandy Marko Knauer <sandy.marko.k@gmail.com>

- Markdown loading fix
- Add ToolBar Icons
- LessionPage refactored (read all from ram)
- LessionPage Switching to the RAM model (Model/View)
- DatabaseManager new function getFilteredFiles 
