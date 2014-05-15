migrate_uhs_policy
==================

Custom migrations for use with Migrate 2.0

# Installation #

See dependencies in .info.

Install [QueryPath Parser](http://querypath.org/).

Install LibreOffice for Word to HTML conversion. This gives you the  'soffice' command.

# Notes #

Will migrate data from Word Doc source files.  Relies on .csv files that demonstrate the structure of the Book to be created. See the data in the examples directory.

```
drush cc all
```
The above tells you how to set a variable to the path of the source files.

# Issues/Notes #

* Sheets in CSV Structure/ need to be converted to .csv
* If Word file is updated code won't re-export to HTML.  The process was to replace the UHS_Manuals/ source directory wholesale. If there is no corresponding .html, one will be created, but no updating of .html files implemented.
* Migrating the same source IDs multiple times should result in content being updated. But here you will get duplicates in your book index. Workaround is to migrate-rollback and reimport the whole book.
* "Used in Multiple Manuals" was renamed to "Shared Book Pages" for clarity.  To avoid find replace in all CSV files:
```
ln -s Shared\ Book\ Pages.csv "Used in Multiple Manuals.csv"
ln -s Shared\ Book\ Pages.csv "Used in Multiple Manuals.csv"
# ...or similar solution
```
* If you are seeing random QueryParser errors (source_parser.inc) it can be helpful to uncomment this link in book_files.inc: prepareRow() to determine what source row is causing the problem:
```
      //DEBUG: find ids that make qp bork.
      //print $row->source_id . "\n";
```