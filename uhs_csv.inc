<?php

class MigrateSourceUhsCsv extends MigrateSourceCSV {

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
  public function __construct($path, array $csvcolumns = array(), array $options = array(), array $fields = array()) {
    parent::__construct($path, $csvcolumns, $options, $fields);
  }

}