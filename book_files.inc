<?php

class UhsMigration extends Migration {
  public $base_dir;

  /**
   * Constructor.
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    // A map of source HTML filename -> destination node id.
    $this->map = new MigrateSQLMap($this->machineName,
        array(
          'sourceid' => array(
            'type' => 'varchar',
            'length' => 255,
            'not null' => TRUE,
          )
        ),
        MigrateDestinationNode::getKeySchema()
    );

    // The source fields.
    $fields = array(
      'title' => t('Title'),
      'body' => t('Body'),
      'uid' => t('User id'),
    );

    // Since the base directory of the HTML files can change depending on the
    // environment, we keep it in a variable. There is no interface for this,
    // set it using drush vset.
    $this->base_dir = variable_get('uhs_migration_source', '');

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

    // Map the fields, pretty straightforward in this case.
    $this->addFieldMapping('uid', 'uid');
    $this->addFieldMapping('title', 'title');
    $this->addFieldMapping('body', 'body')
      ->arguments(array('format' => 'full_html'));
  }

  /**
   * Prepare a row.
   */
  public function prepareRow($row) {
    // Set to admin for now.
    $row->uid = 1;

    // Create a new SourceParser to handle HTML content.
    $source_parser = new SourceParser(substr($row->sourceid, 1), $row->filedata);
    $row->body = $source_parser->getBody();

    // The title is the filename.
    $row->title = $row->sourceid;
  }
}