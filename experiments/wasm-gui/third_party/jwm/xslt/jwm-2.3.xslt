<?xml version="1.0"?>
<!-- XSLT to update a JWM 2.2 configuration to 2.3.
     To convert a JWM configuration file using this XSLT and xsltproc:

        xsltproc jwm-2.3.xsl oldjwmrc > newjwmrc

     All relevant JWM configuration files will need to be updated.
     Note: auto-hide trays are assumed to hide to the bottom of the screen.
-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <xsl:output method="xml" indent="yes"/>

    <!-- Copy everything by default. -->
    <xsl:template match="@*|node()">
        <xsl:copy>
            <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
    </xsl:template>

    <!-- Convert autohide attribute for Tray elements. -->
    <xsl:template match="@autohide">
        <xsl:attribute name="autohide">
            <xsl:choose>
                <xsl:when test=". = 'true'">
                    <xsl:text>bottom</xsl:text>
                </xsl:when>
                <xsl:when test=". = 'false'">
                    <xsl:text>off</xsl:text>
                </xsl:when>
                <xsl:otherwise><xsl:apply-templates/></xsl:otherwise>
            </xsl:choose>
        </xsl:attribute>
    </xsl:template>
    <!-- Prefix the contents of Clock tags with exec within a Button tag. -->
    <xsl:template match="Clock">
        <Clock>
            <xsl:apply-templates select="@*"/>
            <Button mask="123">exec:<xsl:value-of select="."/></Button>
        </Clock>
    </xsl:template>

    <!-- Rename Text to Foreground. -->
    <xsl:template match="Text">
        <Foreground><xsl:value-of select="."/></Foreground>
    </xsl:template>

    <!-- Rename Title to Background. -->
    <xsl:template match="Title">
        <Background><xsl:value-of select="."/></Background>
    </xsl:template>

    <!-- Move the contents of WindowStyle/Inactive to WindowStyle. -->
    <xsl:template match="WindowStyle">
        <xsl:copy>
            <xsl:apply-templates select="Inactive/node()"/>
            <xsl:apply-templates select="child::node()[not(self::Inactive)]"/>
        </xsl:copy>
    </xsl:template>

    <!-- Move ActiveBackground/ActiveForeground to Active. -->
    <xsl:template name="Active">
        <xsl:apply-templates select="@*|node()"/>
        <xsl:if test="ActiveBackground or ActiveForeground">
            <Active>
                <xsl:if test="ActiveBackground">
                    <Background>
                        <xsl:value-of select="ActiveBackground"/>
                    </Background>
                </xsl:if>
                <xsl:if test="ActiveForeground">
                    <Foreground>
                        <xsl:value-of select="ActiveForeground"/>
                    </Foreground>
                </xsl:if>
            </Active>
        </xsl:if>
    </xsl:template>

    <!-- Handle TrayStyle -->
    <xsl:template match="TrayStyle">
        <TrayStyle>
            <xsl:call-template name="Active"/>
        </TrayStyle>
    </xsl:template>

    <!-- Handle TrayButtonStyle -->
    <xsl:template match="TrayButtonStyle">
        <TrayStyle>
            <xsl:call-template name="Active"/>
        </TrayStyle>
    </xsl:template>

    <!-- Handle TaskListStyle -->
    <xsl:template match="TaskListStyle">
        <TrayStyle>
            <xsl:call-template name="Active"/>
        </TrayStyle>
    </xsl:template>

    <!-- Handle PagerStyle -->
    <xsl:template match="PagerStyle">
        <PagerStyle>
            <xsl:call-template name="Active"/>
        </PagerStyle>
    </xsl:template>

    <!-- Handle MenuStyle -->
    <xsl:template match="MenuStyle">
        <MenuStyle>
            <xsl:call-template name="Active"/>
        </MenuStyle>
    </xsl:template>

</xsl:stylesheet>
