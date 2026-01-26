# SonarPractice (Alpha)

Organize your repertoire and track your practice progress with SonarPractice – developed by a musician for musicians.


## Background & Motivation

SonarPractice originated from a very personal need. As a musician, I was looking for an efficient way to organize my repertoire and systematically track my practice progress.

Paper is forgiving, but inflexible. Spreadsheets turned out to be too cumbersome and simply uncomfortable to use in daily practice. Since no existing software met my requirements in terms of performance and structure, I decided to develop SonarPractice myself.

## Key Features

- High-Performance Media Catalog: Lightning-fast management of over 20,000 files using an SQLite backend with dedicated indexing on file paths.

-  Intelligent Repertoire Management: Linking master songs (e.g. Guitar Pro files) with accompanying materials such as audio backings or PDF sheets.

-  Smart Search: Real-time filtering within the catalog with instant feedback.

-  Practice Journal: Calendar-based tracking of BPM values, notes, and practice duration to monitor progress.

-  Responsive UI: Uninterrupted workflow thanks to asynchronous loading processes and progress indicators.

## Technical Stack

- Framework: Qt 6.10 (modern signal/slot syntax and UI optimizations).

- Language Standard: C++23 (use of modern features such as std::as_const for clean code).

- Database: SQLite 3 with optimized indices for fast data access even with large libraries.

- Architecture: Consistent use of the Model/View pattern to separate data and presentation.

## Build Instructions

The project requires a C++23-capable compiler (MSVC 2022, GCC 12+, or Clang 15+) and Qt 6.10.

The database structure, including all performance-related indices, is automatically initialized in the background on the first application start.

## License & Open Source

I decided to release SonarPractice as an open-source project under the GNU GPLv3 license.

I strongly believe that this tool can also help other musicians structure their daily practice routines and better manage their repertoire. By making the source code publicly available, I want to give something back to the musician and developer community and create a platform for shared growth.

- Community: Everyone is invited to use the tool, improve it, and contribute their own ideas.

- Transparency: Thanks to GPLv3, SonarPractice will remain free and open for everyone in the long term.

## Planned Features (Roadmap)

SonarPractice is under continuous development. The following features are planned for upcoming versions:

[] Intelligent Guitar Pro Parsing: Automatic extraction of metadata (artist, tuning, BPM) from .gp files for automatic cataloging.

[] Setlist Management: Creation of targeted song lists for gigs or specific practice sessions.

[] Extended File Management: Option to move files (instead of only copying) for a cleaner folder structure.

[] Detailed Statistics: Graphical evaluation of progress (e.g. BPM increase over time).

[] PDF Export: Export functionality for journal entries and setlists.

[] Performance Boost: Further SQL optimizations for libraries well beyond 20,000 entries.

## Support / Buy Me a Coffee

SonarPractice is a passion project that was created over countless hours after work.

If the program helps you reach your goals on your instrument faster,
I’d appreciate a small contribution:

[Quickly and easily buy me a cup of coffee.](https://buymeacoffee.com/sonarpractice)

[Direct support for further development.](paypal.me/SandyMarkoKnauer739)

Thank you very much for your support!

[README_de.md (German Vesion)](README_de.md)

