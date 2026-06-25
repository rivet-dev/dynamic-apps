#!/usr/bin/perl -w

# Convert from UltraEdit syntax definition file format to GtkSourceView's .lang

use strict;

my $language_name = "";
my $string_chars = "";
my $escape_char = "";
my $case_sensitive = 1;
my @line_comments = ();
my $block_comment_on = "";
my $block_comment_off = "";
my %classes;

my $l_seen = 0;
my $this_class_name = "";
my @this_class_keywords = ();

######################################################################
# Parsing

while (<>) {
    my $line = $_;
    
    if ($line =~ /^\/L[0-9]+\"([^\"]+)\"/) {
	$l_seen = 1;
	$language_name = $1;
	&parse_language_line ($line);
	next;
    }

    die "Not a proper UltraEdit syntax file\n" if (!$l_seen);

    # Chop trailing whitespace
    $line =~ s/\s+$//;

    # Skip unhandled control lines and empty lines
    # FIXME: handle "Function String" at the very least and generate pattern items
    next if ($line =~ /^\/(Delimiters|Function String|Indent String|Unindent String)/);
    next if ($line eq "");

    if ($line =~ /^\/C([0-9]+)\s*(.*)/) {
	my $new_class_number = $1;
	my $new_class_name = $2;

	# Save old class first
	if ($this_class_name ne "") {
	    # We need to copy the array, since we're going to store a reference to it
	    my @keywords_copy = @this_class_keywords;
	    $classes{$this_class_name} = \@keywords_copy;
	}
	
	$this_class_name = $new_class_name eq "" ? $new_class_number : $new_class_name;
	# Strip quotes from class name
	$this_class_name =~ s/^\"?(.+)\"$/$1/;
	@this_class_keywords = ();
	next;
    }

    if ($this_class_name eq "") {
	print "I don't have a class to add the keywords to, at line $.\n";
	next;
    }

    # Add keywords to the current class
    my @keys = split /\s+/, $line;
    push @this_class_keywords, @keys;
}


######################################################################
# Output

&xml_reset;

&xml_print ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
&xml_print ("<!-- <!DOCTYPE language SYSTEM \"language.dtd\"> -->\n");
# FIXME: what to do with the mime types?
&xml_print ("<language name=\"$language_name\" ",
	    "version=\"1.0\" section=\"Sources\" ",
	    "mimetypes=\"text/x-whatever\">\n");
&xml_enter;

# Line comments
foreach my $line_comment (@line_comments) {
    &xml_print ("<line-comment name=\"Line Comment\" style=\"Comment\">\n");
    &xml_enter;
    &xml_print ("<start-regex>", &regex_xml_quote ($line_comment), "</start-regex>\n");
    &xml_leave;
    &xml_print ("</line-comment>/\n");
} 

# Block comments
if ($block_comment_on ne "") {
    &xml_print ("<block-comment name=\"Block Comment\" style=\"Comment\">\n");
    &xml_enter;
    &xml_print ("<start-regex>", &regex_xml_quote ($block_comment_on), "</start-regex>\n");
    &xml_print ("<end-regex>", &regex_xml_quote ($block_comment_off), "</end-regex>\n");
    &xml_leave;
    &xml_print ("</block-comment>/\n");
}

# Strings
foreach my $string_delimiter (split / */, $string_chars) {
    &xml_print ("<string name=\"String\" style=\"String\" end-at-line-end=\"TRUE\">\n");
    &xml_enter;
    &xml_print ("<start-regex>", &regex_xml_quote ($string_delimiter), "</start-regex>\n");
    &xml_print ("<end-regex>", &regex_xml_quote ($string_delimiter), "</end-regex>\n");
    &xml_leave;
    &xml_print ("</string>\n");
} 

# Keyword classes
foreach my $class (keys %classes) {
    &xml_print ("<keyword-list name=\"$class\" style=\"Keyword\" case-sensitive=\"",
		$case_sensitive ? "TRUE" : "FALSE", "\">\n");
    &xml_enter;
    foreach my $key (@{$classes{$class}}) {
	&xml_print ("<keyword>", &xml_quote ($key), "</keyword>\n");
    }
    &xml_leave;
    &xml_print ("</keyword-list>\n");
}

# Remaining pattern items 

# FIXME: this is intended to output pattern items with elements from
# classes which can't be expressed as keywords

&xml_leave;
&xml_print ("</language>\n");


######################################################################
# Auxiliary functions

sub parse_language_line
{
    my $line = shift;
    my @parts = split / /, $line;

    while (@parts) {
	my $part = shift @parts;

	# Handle single words first
	if ($part eq "Nocase") {
	    $case_sensitive = 0;
	    next;
	}
	# Handle argument extended phrases
	elsif ($part =~ /Line|Block|File|Escape|String/) {
	    # Eat up @parts until the equal sign
	    while (@parts) {
		my $next_part = shift @parts;
		if ($next_part eq "=") {
		    last;
		}
		$part .= " $next_part";
	    }
	}
	else {
	    next;
	}

	last if ($part eq "File Extensions");

	# Get the argument
	my $argument = shift @parts;
	if ($part =~ /Line Comment|Line Comment Alt/) {
	    push @line_comments, $argument;
	}
	elsif ($part eq "Block Comment On") {
	    $block_comment_on = $argument;
	}
	elsif ($part eq "Block Comment Off") {
	    $block_comment_off = $argument;
	}
	elsif ($part eq "Escape Char") {
	    # Not yet supported in GtkSourceView
	    $escape_char = $argument;
	}
	elsif ($part eq "String Chars") {
	    $string_chars = $argument;
	}
	else {
	    print "Unknown phrase $part\n";
	}
    }
}

my $xml_indent_level;

sub xml_reset  { $xml_indent_level = 0; }
sub xml_enter  { $xml_indent_level += 1; }
sub xml_leave  { $xml_indent_level -= 1; }
sub xml_indent { print "\t" x $xml_indent_level; }
sub xml_print  { &xml_indent; print @_; }

sub xml_quote
{
    $_ = $_[0];
    s/\&/\&amp;/g;
    s/\</\&lt;/g;
    s/\>/\&gt;/g;
    s/\"/\&quot;/g;
    return $_;
}

sub regex_quote
{
    $_ = $_[0];
    s/\*/\\*/g;
    return $_;
}

sub regex_xml_quote
{
    return &xml_quote (&regex_quote ($_[0]));
}
