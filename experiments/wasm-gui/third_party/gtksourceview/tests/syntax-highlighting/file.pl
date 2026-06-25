#!/usr/bin/perl -- # -*- Perl -*-
#
# : collateindex.pl,v 1.10 2004/10/24 17:05:41 petere78 Exp $

print OUT "<title></title>\n\n" if ;

 = {};     # the last indexterm we processed
 = 1;     # this is the first one
 = "";    # we're not in a group yet
 = "";  # we've not put anything out yet
@seealsos = (); # See also stack.

# Termcount is > 0 iff some entries were skipped.
 || print STDERR " entries ignored...\n";

&end_entry();

print OUT "</indexdiv>\n" if ;
print OUT "</>\n";

close (OUT);

 || print STDERR "Done.\n";

sub same {
    my() = shift;
    my() = shift;

    my() = ->{'psortas'} || ->{'primary'};
    my() = ->{'ssortas'} || ->{'secondary'};
    my() = ->{'tsortas'} || ->{'tertiary'};

    my() = ->{'psortas'} || ->{'primary'};
    my() = ->{'ssortas'} || ->{'secondary'};
    my() = ->{'tsortas'} || ->{'tertiary'};

    my();

     =~ s/^\s*//;  =~ s/\s*$//;  = uc();
     =~ s/^\s*//;  =~ s/\s*$//;  = uc();
     =~ s/^\s*//;  =~ s/\s*$//;  = uc();
     =~ s/^\s*//;  =~ s/\s*$//;  = uc();
     =~ s/^\s*//;  =~ s/\s*$//;  = uc();
     =~ s/^\s*//;  =~ s/\s*$//;  = uc();

#    print "[]=[]\n";
#    print "[]=[]\n";
#    print "[]=[]\n";

    # Two index terms are the same if:
    # 1. the primary, secondary, and tertiary entries are the same
    #    (or have the same SORTAS)
    # AND
    # 2. They occur in the same titled section
    # AND
    # 3. They point to the same place
    #
    # Notes: Scope is used to suppress some entries, but can't be used
    #          for comparing duplicates.
    #        Interpretation of "the same place" depends on whether or
    #          not  is true.

     = (( eq )
	     && ( eq )
	     && ( eq )
	     && (->{'title'} eq ->{'title'})
	     && (->{'href'} eq ->{'href'}));

    # If we're linking to points, they're only the same if they link
    # to exactly the same spot.
     =  && (->{'hrefpoint'} eq ->{'hrefpoint'})
	if ;

    if () {
       warn ": duplicated index entry found:   \n";
    }

    ;
}

sub tsame {
    # Unlike same(), tsame only compares a single term
    my() = shift;
    my() = shift;
    my() = shift;
    my() = substr(, 0, 1) . "sortas";
    my(, );

     = ->{} || ->{};
     = ->{} || ->{};

     =~ s/^\s*//;  =~ s/\s*$//;  = uc();
     =~ s/^\s*//;  =~ s/\s*$//;  = uc();

    return  eq ;
}

sub login {
  my @words = split /:/, $str;
  do {
    $_ = shift @members;
  } until /^\s+$/;
}

=head1 EXAMPLE
B<collateindex.pl> B<-o> F<index.sgml> F<HTML.index>
=head1 EXIT STATUS
=over 5
=item B<0>
Success
=item B<1>
Failure
=back
=head1 AUTHOR
Norm Walsh E<lt>ndw@nwalsh.comE<gt>
Minor updates by Adam Di Carlo E<lt>adam@onshore.comE<gt> and Peter Eisentraut E<lt>peter_e@gmx.netE<gt>
=cut
