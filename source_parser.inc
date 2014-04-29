<?php

// Include QueryPath.
$path = drupal_get_path('library', 'querypath'); //requires hook_libraries_info?
require_once 'sites/all/libraries/querypath/src/qp.php';

//require_once drupal_get_path('module', 'my_migration') . '/libraries/QueryPath-2.1.2-minimal/Extension/QPXML.php';

class SourceParser {
  //protected $id;
  protected $html;
  //protected $qp;
  public $qp;

  /**
   * Constructor.
   *
   * @param $id
   *  The id from the csv index data
   * @param $html
   *  The full HTML data as loaded from the file.
   */
  //public function __construct($id, $html) {
  public function __construct($html) {
    $this->id = $id;
    $this->html = $html;
    /*
        $this->html = <<<EOT
    <P CLASS="western" STYLE="margin-bottom: 0in"><BR>
    Stuff
    </P>
    <DIV TYPE=FOOTER>
      <P STYLE="margin-bottom: 0in; widows: 0; orphans: 0"><SDFIELD TYPE=FILENAME FORMAT=PATHNAME>/Users/bwood/Work/drupal/Site-backups/UHS/UHS_Manuals/Allergy/Organization.doc</SDFIELD></P>
    </DIV>
    EOT;
    ;
    */
    /*
        $this->charTransform();
        $this->fixEncoding();
        $this->wrapHTML();
        $this->initQP();
        $this->stripComments();
    */
    $this->initQP();
    $this->removeFooter();
    $this->removeTitles();
    $this->removeBlankLines();
    $this->removePinLi();
  }

  /**
   * Replace characters.
   */
  protected function charTransform() {
    // We need to strip the Windows CR characters, because otherwise we end up
    // with &#13; in the output.
    // http://technosophos.com/content/querypath-whats-13-end-every-line
    $this->html = str_replace(chr(13), '', $this->html);
  }

  /**
   * Deal with encodings.
   */
  protected function fixEncoding() {
    // If the content is not UTF8, we assume it's WINDOWS-1252. This fixes
    // bogus character issues. Technically it could be ISO-8859-1 but it's safe
    // to convert this way.
    // http://en.wikipedia.org/wiki/Windows-1252
    $enc = mb_detect_encoding($this->html, 'UTF-8', TRUE);
    if (!$enc) {
      $this->html = mb_convert_encoding($this->html, 'UTF-8', 'WINDOWS-1252');
    }
  }

  /**
   * Wrap an HTML fragment in the correct head/meta tags so that UTF-8 is
   * correctly detected, and for the parsers and tidiers.
   */
  protected function wrapHTML() {
    // We add surrounding <html> and <head> tags.
    $html = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">';
    $html .= '<html xmlns="http://www.w3.org/1999/xhtml"><head><meta http-equiv="Content-Type" content="text/html; charset=utf-8" /></head><body>';
    $html .= $this->html;
    $html .= '</body></html>';
    $this->html = $html;
  }

  /**
   * Create the QueryPath object.
   */
  protected function initQP() {
    $qp_options = array(
      'convert_to_encoding' => 'utf-8',
      'convert_from_encoding' => 'utf-8',
      'strip_low_ascii' => FALSE,
    );
    $this->qp = htmlqp($this->html, NULL, $qp_options);
  }

  /**
   * Remove the comments from the HTML.
   */
  protected function stripComments() {
    foreach ($this->qp->top()->xpath('//comment()')->get() as $comment) {
      $comment->parentNode->removeChild($comment);
    }
  }

  public function removeFooter() {
    // remove <DIV TYPE=FOOTER>
    $this->qp->find('div[title=footer]')->after('<p></p>');
    $this->qp->remove('div[title=footer]'); //attribute value is case sensitive
  }

  public function removeTitles() {
    /*
     * Remove
     *   <p style=...border-bottom: 1.50pt solid #c0c0c0c0;...>
     *   ...
     *   </p>
     */
    foreach ($this->qp->find('p') as $p) {
      $style = $p->attr("style");
      if (strpos($style, "border-bottom: 1.50pt solid #c0c0c0;") !== FALSE) {
        $p->remove();
      }
      /*
       * Remove
       *   <p style=...border-bottom: 1px solid #000000;...>
       *     <font ... >Foo bar Page x of x</font>
       *   </p>
       */

      //if (preg_match('/border-bottom: .+ solid #000000;/', $style) == 1) {
      if (strpos($style, 'border-bottom: 1px solid #000000;') !== FALSE) {
        $match = FALSE;
        $font_tags = $p->html();
        foreach ($p->deepest('font') as $c) {
          if (strpos($c->text(), 'Page ') !== FALSE) {
            $match = TRUE;
          }
        }
        /*
         * Remove
         *   <p style=...border-bottom: 1px solid #000000;...>
         *     <br>
         *     <br>
         *   </p>
         */
        $count = 0;
        foreach ($p->children() as $c) {
          $c1 = $c->html();
          // the html source is just <br>, but qp gives us <br/>
          if (trim($c1) == '<br/>') {
            $count++;
          }
        }
        if (($match) || ($count == 2)) {
          $p->remove();
        }
      }
    }
  }

  /*
   * <p><br /></p>
   * unnecessary whitespace. remove <p> and child.
   */
  public function removeBlankLines() {
    $this->qp->find('br:only-child')->parent()->remove();
  }

  /*
   * <li><p>foo</p></li>
   * unnecessary whitespace. remove the <p> tags.
   */
  public function removePinLi() {
    //foreach ($this->qp->find('p:only-child')->parent('li') as $list_item) {
    foreach ($this->qp->find('li') as $list_item) {
      $content = $list_item->children('p')->children()->html();
      if (!empty($content)) {
        $x = $list_item->html();
        $list_item->children('p')->remove();
        $list_item->html($content);
      }
    }
  }

  /**
   * Return the HTML.
   */
  public function getBody() {
    $body = $this->qp->top('html')->innerHTML();
    $body = trim($body);
    return $body;
  }
}