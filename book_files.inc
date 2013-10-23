<?php

class UhsMigration extends Migration {
  public $base_dir;

  /**
   * Constructor.
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);

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
    );


    /***
    // The source of the migration is HTML files from the old site.
    $list_files = new MigrateListFiles(array($this->base_dir), $this->base_dir, $regex);
    $item_file = new MigrateItemFile($this->base_dir);
    $this->source = new MigrateSourceList($list_files, $item_file, $fields);
    ***/

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
    $path = $arguments['path']; //vget should always give a default value
    (array_key_exists('csvcolumns', $arguments)) ? $csvcolumns = $arguments['csvcolumns'] : $csvcolumns = array();
    (array_key_exists('options', $arguments)) ? $options = $arguments['options'] : $options = array();
    (array_key_exists('fields', $arguments)) ? $fields = $arguments['fields'] : $fields = array();

    $this->source = new MigrateSourceCSV($path, $csvcolumns, $options, $fields);

    // The destination content type.
    $this->destination = new MigrateDestinationNode('book');

    /*** Map the fields ***/

    // source_id don't need to map. Handeled by MigrateSQLMap above

    // Not mapped to destination fields. (Will still be avialable in $row.)
    $this->addUnmigratedSources(array('filename', 'parent_id'));

    //uid not in source data
    $this->addFieldMapping('uid', NULL)
      ->defaultValue(1);
    // body not in source data
    $this->addFieldMapping('body', NULL)
      ->arguments(array('format' => 'panopoly_html_text'))
      ->defaultValue("This is test content...");

    // Title is the only field actually mapped from the CSV.
    $this->addFieldMapping('title', 'title');
  }

  /**
   * Prepare a row.
   */
  public function prepareRow($row) {
    if ((empty($row->title)) || (empty($row->filename)))  {
      return FALSE;
    }

    //if (($row->filename) == 'BOOK') {
      //$row->book_id = $row->source_id;
    //}
    $row->book_id = 1;
    // Set to admin for now.
    //$row->uid = 1;
    // done with default value

    /*
    // Create a new SourceParser to handle HTML content.
    $source_parser = new SourceParser(substr($row->sourceid, 1), $row->filedata);
    $row->body = $source_parser->getBody();

    // The title is the filename.
    $row->title = $row->sourceid;
    */
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

    _book_update_outline($node);
    node_save($node);

  }


}