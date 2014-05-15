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
    $this->initQP();

    $this->removeFooter();
    $this->removeTitles();
    $this->removeBrInP();
    $this->removePinLi();
    $this->removeNegativeMargins();

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
   * Replace high ASCII with which can be searched on and manually corrected
   * later. Could result in mangled content
   */
  protected function stripHighAscii() {
    foreach (str_split($this->html) as $char) {
      $ascii = ord($char);
      if ($ascii > 127) {
        $this->html_old = $this->html;
        $this->html = str_replace(chr($ascii), '', $this->html);
      }
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
      //'convert_from_encoding' => 'utf-8',
      'convert_from_encoding' => '',
      'strip_low_ascii' => FALSE,
      'replace_entities' => TRUE,
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
    if (is_object($this->qp->find('div[title=footer]'))) {
      $this->qp->find('div[title=footer]')->after('<p></p>');
      $this->qp->remove('div[title=footer]'); //attribute value is case sensitive
    }

  }

  public function removeTitles() {
    $this->removeAllergyTitles();
    $this->removeDivTitle();
  }

  public function removeDivTitle() {
    // Bail if the interesting html nodes aren't present
    $div_title = $this->qp->find('div[title=header]');
    $size = $div_title->children()->size();
    if ($size < 1) {
      return;
    }

    $save = '';
    foreach ($div_title->children() as $p) {
      $content = $p->children()->html();
      if ((preg_match('/reviewed/i', $content) > 0) ||
        (preg_match('/effective\W*date/im', $content) > 0)
      ) {
        foreach ($p->children() as $pc) {
          $save .= $pc->html();
        }
      }
    }
    $div_title->children('p')->remove();
    $div_title->html("<p>" . $save . "</p>");
  }

  public function removeAllergyTitles() {
    // Bail if interesting node not present
    if (!is_object($this->qp->find('p'))) {
      return;
    }

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
      if (((strpos($style, 'border-bottom: 1px solid #000000;') !== FALSE)) ||
        (strpos($style, 'border-bottom: 0.75pt solid #000000;') !== FALSE)
      ) {
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
  public function removeBrInP() {

    // Bail if the interesting node is not present
    $node = $this->qp->find('p')->children('br');
    $size = $node->size();
    if ($size < 1) {
      return;
    }

    $content = trim($this->qp->find('br:only-child')->parent('p')->html());
    // Ensure no text in the <p>
    if (empty($content)) {
      $this->qp->find('br:only-child')->parent('p')->remove();
    }
  }

  /*
   * <li><p>foo</p></li>
   * unnecessary whitespace. remove the <p> tags.
   */
  public function removePinLi() {
    // Bail if the interesting node is not present
    $lis = $this->qp->find('li');
    $size = $lis->size();
    if ($size < 1) {
      return;
    }

    /*
     * Given:
     *   <li><p>foobar</p></li>
     * Make it
     *   <li>foobar</li>
     */
    foreach ($this->qp->find('p:only-child') as $list_item) {
      //$x = $list_item->html();
      $parent = $list_item->parent();
      //$y = $parent->html();
      if (!$parent->is('li')) {
        continue;
      }

      //if matched tag has children get them, otherwise get text
      $num_children = $list_item->children()->size();
      if ($num_children > 0) {
        $content = $list_item->children()->html();
      }
      else {
        $content = $list_item->text();
      }
      /*
       * These next two lines might seem unnecessary, but I've found that by
       * encoding and then decoding I filter out weird characters that cause
       * errors.
       */
      $content = htmlentities($list_item->html());
      $content = trim(html_entity_decode($content));
      $list_item->remove();
      $parent->append($content);

    }
  }

  /**
   * Remove margin attributes with negative values
   */
  public function removeNegativeMargins() {
    $ps = $this->qp->find('p');
    $hs = $this->qp->find('h1,h2,h3,h4,h5,h6');
    /*
    $psize = $ps->size();
    $psize = $hs->size();
    if ($size < 1) {
      return;
    }
    */

    foreach (array($ps, $hs) as $tags) {
      if ($tags->size() < 1) {
        continue;
      }
      foreach ($tags as $tag) {
        if ($tag->hasAttr('style')) {
          $clean_value = preg_replace("/((?:margin-left)|(?:margin-right)):\s*-[^;]*;\s*/", '', $tag->attr('style'));
          $tag->attr('style', $clean_value);
        }
      }
    }
  }

  /**
   * Return the HTML.
   */
  public function getBody() {
    /*
     * When the title is empty we end up with a <title/> tag in the <head> and
     * somehow this results in migrate putting the entire document inside a
     * <title> tags which results in a broken blank page.
     */
    $title = $this->qp->find('title')->text();
    if (empty($title)) {
      $this->qp->find('title')->remove();
    }
    //$body = $this->qp->top('html')->innerHTML();
    $body = $this->qp->top('body')->innerHTML();
    //Replace newlines with space.  panopoly_wysiwyg_text converts these to <p> an <br>
    $body = str_replace("\n", ' ', $body);
    $body = str_replace("\t", ' ', $body);
    $body = trim($body);
    return $body;
  }
}