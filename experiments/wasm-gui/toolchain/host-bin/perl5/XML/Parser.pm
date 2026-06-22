# Stub XML::Parser: intltool's configure requires the module to exist, but we build with --disable-nls
# and our intltool-extract/-merge shims do not parse XML, so a no-op package satisfies the check.
package XML::Parser;
sub new { return bless {}, shift }
sub setHandlers { }
sub parse { }
sub parsefile { }
1;
