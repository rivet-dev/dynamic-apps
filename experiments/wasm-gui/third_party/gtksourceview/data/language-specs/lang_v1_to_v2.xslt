<?xml version="1.0" ?> 
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0" >

<xsl:output method="xml" indent="yes"/>
<!--<xsl:strip-space elements="*"/>-->


<xsl:template match="/">
<xsl:comment> Automatically converted from language spec v1.0 with 
     lang_v1_to_v2.xslt, written by Emanuele Aina &lt;emanuele.aina@tiscali.it&gt;
</xsl:comment>
<xsl:apply-templates/>
</xsl:template>

<xsl:template match="/language">
<language>
    <xsl:copy-of select="@*"/>
    <xsl:attribute name="version">2.0</xsl:attribute>
    <xsl:if test="@name">
        <xsl:attribute name="id">
            <xsl:value-of
                  select="translate (@name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
        </xsl:attribute>
    </xsl:if>
    <xsl:if test="@_name">
        <xsl:attribute name="id">
            <xsl:value-of
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
         </xsl:attribute>
    </xsl:if>

    <styles>
        <xsl:for-each select="line-comment|block-comment|string|syntax-item|pattern-item|keyword-list">
        <xsl:if test="@style">
            <style>
                <xsl:copy-of select="@_name"/>
                <xsl:attribute name="id">
                    <xsl:value-of 
                      select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
                </xsl:attribute>
                <xsl:attribute name="map-to">
                    <xsl:text>def:</xsl:text>
                    <xsl:value-of 
                      select="translate (@style, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
                </xsl:attribute>
            </style>
        </xsl:if>
        </xsl:for-each>
    </styles>
    <definitions>
        <xsl:if test="/language/escape-char">
        <context id="escape">
            <match><xsl:value-of select="//escape-char"/>.</match>
        </context>
        </xsl:if>

        <context>
            <xsl:attribute name="id">
                <xsl:value-of
                      select="translate (/language/@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
            <include>
            <xsl:apply-templates />
            </include>
        </context>
    </definitions>
</language>
</xsl:template>


<xsl:template match="escape-char">
    <!-- suppressed -->
</xsl:template>

<xsl:template match="line-comment">
    <context>
        <xsl:if test="@style">
            <xsl:attribute name="style-ref">
                <xsl:value-of 
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
        </xsl:if>
        <xsl:comment> Name: <xsl:value-of select="@_name" /> </xsl:comment>
        <match>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="start-regex" />
            <xsl:text>.*$/</xsl:text>
        </match>
    </context>
</xsl:template>

<xsl:template match="block-comment">
    <context>
        <xsl:if test="@style">
            <xsl:attribute name="style-ref">
                <xsl:value-of
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
        </xsl:if>
        <xsl:comment> Name: <xsl:value-of select="@_name" /> </xsl:comment>
        <start>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="start-regex" />
            <xsl:text>/</xsl:text>
        </start>
        <end>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="end-regex" />
            <xsl:text>/</xsl:text>
        </end>
    </context>
</xsl:template>


<xsl:template match="string">
    <context>
        <xsl:if test="@style">
            <xsl:attribute name="style-ref">
                <xsl:value-of
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
        </xsl:if>
        <xsl:comment> Name: <xsl:value-of select="@_name" /> </xsl:comment>
        <start>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="start-regex" />
            <xsl:text>/</xsl:text>
        </start>
        <end>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="end-regex" />
            <xsl:text>/</xsl:text>
        </end>
        <include>
            <context ref="escape" />
        </include>
    </context>
</xsl:template>


<xsl:template match="syntax-item">
    <context>
        <xsl:if test="@style">
            <xsl:attribute name="style-ref">
                <xsl:value-of
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
        </xsl:if>
        <xsl:comment> Name: <xsl:value-of select="@_name" /> </xsl:comment>
        <start>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="start-regex" />
            <xsl:text>/</xsl:text>
        </start>
        <end>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="end-regex" />
            <xsl:text>/</xsl:text>
        </end>
    </context>
</xsl:template>


<xsl:template match="keyword-list">
    <context>
        <xsl:if test="@style">
            <xsl:attribute name="style-ref">
                <xsl:value-of
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
        </xsl:if>

        <xsl:comment> Name: <xsl:value-of select="@_name" /> </xsl:comment>

        <xsl:variable name="begin-empty" select="not(@match-empty-string-at-beginning='FALSE')"/>
        <xsl:variable name="end-empty" select="not(@match-empty-string-at-end='FALSE')"/>

        <xsl:choose>
            <xsl:when test="@beginning-regex">
                <prefix>
                    <xsl:if test="$begin-empty">\b</xsl:if>
                    <xsl:value-of select="@beginning-regex"/>
                </prefix>
            </xsl:when>
            <xsl:otherwise>
                <xsl:if test="not($begin-empty)">
                    <prefix />
                </xsl:if>
            </xsl:otherwise>
        </xsl:choose>
        <xsl:choose>
            <xsl:when test="@end-regex">
                <suffix>
                    <xsl:if test="$end-empty">\b</xsl:if>
                    <xsl:value-of select="@end-regex"/>
                </suffix>
            </xsl:when>
            <xsl:otherwise>
                <xsl:if test="not($end-empty)">
                    <suffix />
                </xsl:if>
            </xsl:otherwise>
        </xsl:choose>

        <xsl:for-each select="keyword">
            <keyword>
                <xsl:value-of select="."/>
            </keyword>
        </xsl:for-each>
    </context>
</xsl:template>


<xsl:template match="pattern-item">
    <context>
        <xsl:if test="@style">
            <xsl:attribute name="style-ref">
                <xsl:value-of
                  select="translate (@_name, ' \/ABCDEFGHIJKLMNOPQRSTUVWXYZ', '---abcdefghijklmnopqrstuvwxyz')"/>
            </xsl:attribute>
        </xsl:if>

        <xsl:comment> Name: <xsl:value-of select="@_name" /> </xsl:comment>

        <match>
            <xsl:text>/</xsl:text>
            <xsl:value-of select="regex" />
            <xsl:text>/</xsl:text>
        </match>
    </context>
</xsl:template>

<xsl:template match="comment()">
  <xsl:copy />
</xsl:template>

</xsl:stylesheet>
