<?php
$i = 1;
$數 = 1;
$i數 = 1;

$str <<<extrémité
text
extrémité

echo <<<"END"
      a
     b
    c
    END;

stringManipulator(<<<'END'
   a
  b
 c
END);

$values = [<<<END
a
b
ENDING
END, 'd e f'];
?>

<!-- PHP embedded: -->

<!-- Inside JavaScript -->
<script>
var one = '<?php echo 'Hello World'; ?>';
var two = '<?php echo "Hello World"; ?>';
var thr = "<?php echo 'Hello World'; ?>";
var fou = "<?php echo "Hello World"; ?>";

var num = <?php echo '3'; ?>;
var re  = /<?php echo 'Hello World'; ?>/;
var tpl = `<?php echo 'Hello World'; ?>`;
</script>

<!-- Inside CSS -->
<style>
@import url(<?php echo $css_url; ?>);
@import url('<?php echo $another_url; ?>');
@import url("<?php echo $some_other_url; ?>");

h1 {
    border-<?php echo $dir; ?>-color: black;
    color: <?php echo $color; ?>;
}
</style>

<!-- Inside HTML -->
<img src='<?php echo $Image['one']; ?>' />
<img src='<?php echo $Image["two"]; ?>' />
<img src="<?php echo $Image['thr']; ?>" />
<img src="<?php echo $Image["fou"]; ?>" />
