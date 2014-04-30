<?php

class UhsMigration extends Migration {
  public $base_dir;
  protected $logfile = '';

  /**
   * Constructor.
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    // logfile

    // A map of source CSV source_id -> destination node id.
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'source_id' => array(
          'type' => 'int',
          'not null' => TRUE,
          'description' => 'Document ID.'
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // The source fields.
    $fields = array(
      'source_id' => t('Source ID of the document.'),
      'title' => t('Title'),
      'filename' => t('Filename of the document.'),
      'parent_id' => t('Parent ID of the document.'),
      'source_manual' => t('Name of the external manual that contains this document.'),
    );


    /**
     * Simple initialization.
     *
     * @param string $path
     *  The path to the source file
     * @param array $csvcolumns
     *  Keys are integers. values are array(field name, description).
     * @param array $options
     *  Options applied to this source.
     * @param array $fields
     *  Optional - keys are field names, values are descriptions. Use to override
     *  the default descriptions, or to add additional source fields which the
     *  migration will add via other means (e.g., prepareRow()).
     */

    if (strtolower(variable_get('uhs_source_data_macos')) == "yes") {
      ini_set("auto_detect_line_endings", "1");
    }

    $this->uhs_migration_source_dir = variable_get('uhs_migration_source', '/tmp');
    $path = $arguments['path']; //vget should always give a default value

    (array_key_exists('csvcolumns', $arguments)) ? $csvcolumns = $arguments['csvcolumns'] : $csvcolumns = array();
    (array_key_exists('options', $arguments)) ? $options = $arguments['options'] : $options = array();
    (array_key_exists('fields', $arguments)) ? $fields = $arguments['fields'] : $fields = array();

    /*
     * Remove empty lines if any exist
     * Empty lines will cause
     * Migration failed with source plugin exception
     * : SQLSTATE[HY000]: General error:[error]
     * 1366 Incorrect integer value: '' for column 'sourceid1' at row 1
     */

    $csv_contents = file($path);
    $csv_contents_clean = array();
    foreach ($csv_contents as $line) {
      if (preg_match("/^,+/", $line) === 0){
        $csv_contents_clean[] = $line;
      }
      else {
        $x=1;
      }
    }

    if (count(array_diff($csv_contents, $csv_contents_clean)) > 0) {
      $fh = fopen($path, 'w');
      foreach ($csv_contents_clean as $line) {
        fwrite($fh, $line);
      }
      fclose($fh);
      $this->uhsSetMessage("Removed empty lines in $path", "INFO");
    }

    $csv_contents_clean = file_get_contents($path);
    // fix character encoding if necessary
    $enc = mb_detect_encoding($csv_contents_clean, 'UTF-8');
    if (!$enc) {
      $csv_contents = mb_convert_encoding($csv_contents_clean, 'UTF-8', '');
      $fh = fopen($path, 'w');
      fwrite($fh, $csv_contents_clean);
      fclose($fh);
      $this->uhsSetMessage("Fixed character encoding in $path");
    }


    $this->source = new MigrateSourceCSV($path, $csvcolumns, $options, $fields);

    // The destination content type.
    $this->destination = new MigrateDestinationNode('book');

    //Keep track of machine_name to filename translations for all migrations
    $this->translations = $arguments[translations];

    /*** Map the fields ***/

    // source_id don't need to map. Handeled by MigrateSQLMap above

    // Not mapped to destination fields. (Will still be avialable in $row.)
    $this->addUnmigratedSources(array('filename', 'parent_id', 'source_manual'));

    // Destinations that that don't map to sources
    // $this->addUnmigratedDestinations(array('field_shared_page'));

    //uid not in source data
    $this->addFieldMapping('uid', NULL)
      ->defaultValue(1);

    /*
     * shared_page is created in prepareRow()
     */
    $this->addFieldMapping('field_shared_page', 'shared_page')
      /*
       * this will "lazily" create terms as we encounter them
       * i.e. if you input foo and foo isn't in the vocab, it will create the
       * term/tid and populate this termref field with the tid (the value
       * of the term itself lives in the vocab table.
       */
      ->arguments(array('create_term' => TRUE))
      ->defaultValue(NULL);

    // body not in source data
    //FIXME: body, null?
    $this->addFieldMapping('body', 'body')
      ->arguments(array('format' => 'uhs_full_html'))
      ->defaultValue(NULL);

    // Title is the only field actually mapped from the CSV.
    $this->addFieldMapping('title', 'title');
  }

  public function uhsSetMessage($msg, $type) {
    if (!isset($type)) {
      if (strpos($msg, 'Error: ') === FALSE) {
        $msg = "$type: $msg";
      }
    }
    print "\n$msg\n";
  }

  public function preImport() {
    parent::preImport();
    // Code to execute before first article row is imported
    /*
     * Prevents:
     *
Migration failed with source plugin exception: Illegal string offset 'value'                               [error]
File
/Users/bwood/Sites/pantheon/uhs-policy2/profiles/openberkeley/modules/contrib/media/modules/media_wysiwyg/includes/media_wysiwyg.file_usage.inc,
line 95
     */
    module_disable(array('media_wysiwyg'));
    //drupal_get_messages('error', false); //would be nice to kill that set_message
  }

  public function postImport() {
    parent::postImport();
    // Code to execute before first article row is imported
    module_enable(array('media_wysiwyg'));
    //drupal_get_messages('error', false); //would be nice to kill that set_message
  }

  public function translateAscii($string, $row) {

    /*
     * Don't work on 'filename' by defualt. Event if filename has ext ascii
     * it will match a filename value with the same ext ascii in another sheet
     * which we want.
     */

    /*
     * Excel uses ANSI encoding...there are weird chars in the titles of some
     * rows.
     *
     * If we could figure out the proper use of
     *   mb_convert_encoding($csv_contents_clean, 'UTF-8', '')
     * and/or
     *   $convmap = array(0x80, 0xff, 0, 0xff); //understand this...
     *   $str = mb_encode_numericentity($row->title, $convmap, "ISO-8859-1");
     * this replacement might not be necessary.
     */

    // Charters that you don't want to see on the site
    $ugly_char_map = array(
      131 => '=F',
      144 => '=144',
      202 => '=E',
      205 => '=I',
      209 => '=N',
      227 => '=227',
      230 => '=A',
      234 => '=234',
      238 => '=I',
      241 => '=n',
      244 => '=244',
    );

    // Characters that are okay to see on the site
    $char_map = array(
      148 => '"',
      208 => '--',
      210 => '"',
      211 => '"',
      213 => "'",
    );

    $ext_char_map = $char_map + $ugly_char_map;

    foreach (str_split($string) as $char) {
      $ascii = ord($char);
      if ($ascii > 127) {
        $string_old = $string;
        if (!in_array($ascii, array_keys($ext_char_map))) {
          $this->uhsSetMessage("========> Error: source_id=" . $row->source_id . ": Extended ASCII characters found ($ascii)", "error");
          return FALSE;
        }
        else {
          $string = str_replace(chr($ascii), $ext_char_map[$ascii], $string);
        }
        // Notify the user about ugly chars in the title.
        if (($string_old == $row->title) && (in_array($ascii, array_keys($ugly_char_map)))) {
          $this->uhsSetMessage("Error: source_id=" . $row->source_id . ": Page title contains extended ASCII characters. Translated to: $string", "error");
        }
      }
    }

    return $string;
  }

  /**
   * Prepare a row.
   */
  public function prepareRow($row) {
    if ((empty($row->title)) || (empty($row->filename))) {
      return FALSE;
    }

    // remove whitespace that could exist in the spreadsheet.
    $elements = get_object_vars($row);
    while (list($k, $v) = each($elements)) {
      $row->$k = trim($v);
    }

    /*
     * Since each migration processes one CSV file which represents one book,
     * book_id can always be 1.
     */
    $row->book_id = 1;

    /*
     * Deal with weird ascii characters
     */
    $row->title = $this->translateAscii($row->title, $row);
    $row->source_manual = $this->translateAscii($row->source_manual, $row);

    /*
     * If title or filename contain "," they should be quoted so that MigrateCSV will correctly parse the csv fields.
     * It's now safe to remove the quotes, and we want to do that so they don't appear in the title field on the website
     * nor in the filename that we are processing.
     */
    $row->title = str_replace('"', '', $row->title);
    $row->filename = str_replace('"', '', $row->filename);

    $file_ext = strtolower(array_pop(explode('.', $row->filename)));
    $ext_supported = array('doc', 'docx');

    // Is this a shared page?
    if (!empty($row->source_manual)) {
      $source_machine_name = $this->translations[$row->source_manual];
      $file_source_id = $this->getSharedPageIdorError($row);
      // shared_page created here. doesn't come from csv file.
      $row->shared_page = NULL;
      if (is_int($file_source_id)) {
        $source_page_nid = parent::handleSourceMigration($source_machine_name, $file_source_id);
        $row->shared_page = $source_page_nid;
      }
      else {
        //If it's not an int, it's some kind of error
        $row->body = "========> Error: Correction needed in CSV Structure: " . $file_source_id;
      }
      return TRUE;
    }

    if ((strpos(strtoupper($row->filename), 'INDEX') !== FALSE) || (strtoupper($row->filename) == 'BOOK')) {
      //Index pages for Allergy manual shouldn't have content
      $row->body = NULL;
    }
    //elseif ((!in_array(strtoupper($row->filename), array('BOOK', 'PDF'))) && $row->parent_id !== 1) {
    elseif ((!in_array(strtoupper($row->filename), array('BOOK', 'PDF'))) && (in_array($file_ext, $ext_supported))) {
      //echo $row->source_id . "\n";
      $sp = new SourceParser($this->getHtml($row));
      $row->body = $sp->getBody();
    }
    else {
      // in this case filename indicates that the row is some type of non-importable content.
      $row->body = "Error: The content for this page is '" . $this->translateAscii($row->filename, $row) . "'.  This type of content will be migrated in a future phase of the project.";
    }
    return TRUE;
  }


  protected function getSharedPageIdorError($row) {
    if (!file_exists($this->uhs_migration_source_dir . "/" . $row->source_manual . ".csv")) {
      $this->uhsSetMessage("source_id=" . $row->source_id . " references source_manual = " . $row->source_manual . "which could not be found.", "error");
      return FALSE;
    }
    $shared_page_manual_handle = fopen($this->uhs_migration_source_dir . "/" . $row->source_manual . ".csv", "r");
    $matches = array();

    while (($data = fgetcsv($shared_page_manual_handle, NULL, ",", '"', '\\')) !== FALSE) {
      if (str_replace('"', '', $data[2]) == $row->filename) {
        $matches[$data[0]] = $row->filename;
      }
    }

    if (count($matches) == 0) {
      $error = "Error: source_id=" . $row->source_id . " references source_manual = " . $row->source_manual . ".csv and filename = " . $row->filename . ", but " . $row->source_manual . ".csv does not contain that file.";
    }
    elseif (count($matches) > 1) {
      //TODO: PDFs, skip etc can't be shared pages
      if (trim(strtolower(array_pop($matches))) !== 'pdf') {
        $error = "Error: source_id=" . $row->source_id . " references source_manual = " . $row->source_manual . ".csv and filename = " . $row->filename . ", and " . $row->source_manual . ".csv lists this file multiple times.";
      }
    }
    elseif (count($matches) == 1) {
      $keys = array_keys($matches);
      return $keys[0];
    }
    $this->uhsSetMessage($error, "error");
    return $error;
  }

  protected function getHtml($row) {
    //TODO: move to .module
    //need to cd to avoid "No Info.plist file in application bundle"
    $path_soffice = 'cd /Applications/LibreOffice.app/Contents/program;'; //vset
    $cmd_soffice = './soffice --headless --convert-to html:HTML';
    $cd_back = '; cd -';

    // Compute base path
    // filename of the csv file == name of its directory
    $parts = explode('/', $this->source->file);
    $dir = str_replace('.csv', '', array_pop($parts));
    $filename = array_shift(explode('.', $row->filename));

    $manual_path = substr($this->source->file, 0, strlen($this->source->file) - 4); //file minus ".cvs" sould be the path to the manual
    if (!is_dir($manual_path)) {
      $error = "No manual directory found at $manual_path";
      $this->uhsSetMessage($error, "error");
      //wrap in body tags so query parser will put it in the node body
      return '<html><body>' . $error . '</body></html>';
    }

    // Look for the file in child directories
    $rd_iterator = new RecursiveDirectoryIterator($manual_path);
    $ritit = new RecursiveIteratorIterator($rd_iterator);
    $source_files = array('html' => array(), 'doc' => array());
    foreach ($ritit as $object_name => $file) {
      //if ($file->isDir()) continue;
      $fn = $file->getFilename();
      if (($fn == '.') || ($fn == '..')) {
        continue;
      }
      if ($file->getFilename() == $filename . '.html') {
        $source_files['html'][] = $file->getPathName();
      }
      if ((strtolower($file->getFilename()) == strtolower($filename . '.doc')) ||
        (strtolower($file->getFilename()) == strtolower($filename . '.docx'))
      ) {
        $source_files['doc'][] = $file->getPathName();
      }
    }

    // File not found
    if ((count($source_files['doc']) == 0) && (count($source_files['html']) == 0)) {
      $error = "Error: source_id=" . $row->source_id . " references file_name = " . $row->filename . " but this file does not exist in the folder " . $manual_path . " nor in any of its sub folders.";
      $this->uhsSetMessage($error, "error");
      //wrap in body tags so query parser will put it in the node body
      return '<html><body>' . $error . '</body></html>';
    }

    // >1 doc file with same name
    if (count($source_files['doc']) > 1) {
      $error = "Error: " . $filename . ".doc found " . count($source_files['doc']) . " times.";
      foreach ($source_files['doc'] as $doc) {
        $ls = lstat($doc);
        $error .= "\n$doc size=" . $ls['size'] . " last changed= " . date("Y-m-d H:i:s", $ls['mtime']);
      }
      $this->uhsSetMessage($error, 'error');
      return '<html><body>' . $error . '</body></html>';
    }

    //is there an HTML version? If no, convert
    if ((count($source_files['doc']) == 1) && (count($source_files['html']) == 0)) {
      //exec(): support filenames and paths containing [space], ', &
      $filewithpath_escaped = str_replace(' ', '\ ', escapeshellcmd(preg_replace('/\.doc[x]?+/i', '', $source_files['doc'][0])));
      $filename_escaped = str_replace(' ', '\ ', escapeshellcmd($filename));
      $docfile_escaped = str_replace(' ', '\ ', escapeshellcmd($source_files['doc'][0]));
      $cmd = $path_soffice . ' ' . $cmd_soffice . ' ' . $docfile_escaped . " --outdir " . str_replace($filename_escaped, '', $filewithpath_escaped);
      $out = exec($cmd, $output, $return_status);
      $html_file = stripslashes($filewithpath_escaped) . ".html";
      if (file_exists($html_file)) {
        return file_get_contents($html_file);
      }
      else {
        $error = "========> Error: Converted html not found at: " . $html_file;
        $this->uhsSetMessage($error, "error");
        return '<html><body>' . $error . '</body></html>';
      }
    }

    // if the .html verison exists, re-import it
    if (count($source_files['html']) == 1) {
      return file_get_contents($source_files['html'][0]);
    }
  }


  /**
   * Acts right after a book node has been saved. Map the book hierarchy.
   *
   * @param object $node
   *   A node object for the newly migrated book.
   * @param stdClass $row
   *   An object representing data from the source row.
   */
  public function complete($node, stdClass $row) {
    // If this is a top-level book, don't set a parent.
    $book_id = ($row->source_id == $row->book_id) ? $node->nid : parent::handleSourceMigration($this->machineName, $row->book_id);

    $node->book['bid'] = $book_id;
    $node->book['nid'] = $node->nid;

    // The CSV will list the pages in the order they should appear, so source_id = weight
    //$node->book['weight'] = $row->book_weight;
    $node->book['weight'] = $row->source_id;

    $parent_id = $row->parent_id;
    /*
    if ($parent_id === NULL) {
      //rows with filename = 'index' have empty parent_id's in some sheets. Assume these should be at the top level of the book.
      $parent_id = $row->book_id;
    }
*/
    // If there is a parent we lookup the migrated node
    if ((isset($parent_id)) && ($parent_id > 0)) {
      $parent_node = parent::handleSourceMigration($this->machineName, $parent_id);
      // We get the mlid of the migrated parent
      $query = db_select("menu_links", "ml")
        ->fields("ml")
        ->condition("ml.link_path", "node/" . $parent_node, "=")
        ->execute()
        ->fetchAssoc();
      $plid = $query['mlid'];

      // We add the parent mlid (plid)
      $node->book['plid'] = $plid;
    }

    /*
     * Only update the book toc and save the node if this nid is not already part of this book
     */
    $query2 = db_select("book", "b")
      ->fields("b")
      ->where("b.nid = :nid AND b.bid = :book_id", array(":nid" => $node->nid, ":book_id" => $book_id))
      ->execute()
      ->fetchAssoc();

    if ($query2 === FALSE) {
      _book_update_outline($node);
      node_save($node);
    }
  }

  protected function createStub($migration, array $source_id) {
    $node = new stdClass();
    $node->title = t('Stub for @id', array('@id' => $source_id[0]));
    $node->body[LANGUAGE_NONE][0] = t('Stub body');
    $node->type = $this->destination->getBundle();
    $node->uid = 1;
    $node->status = 0;
    node_save($node);
    if (isset($node->nid)) {
      return array($node->nid);
    }
    else {
      return FALSE;
    }
  }

}