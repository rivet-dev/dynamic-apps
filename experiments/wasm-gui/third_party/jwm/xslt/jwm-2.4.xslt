<?xml version="1.0"?>
<!-- XSLT to update a JWM 2.3 configuration to 2.4.
     To convert a JWM configuration file using this XSLT and xsltproc:

        xsltproc jwm-2.4.xsl oldjwmrc > newjwmrc

     All relevant JWM configuration files will need to be updated.
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

   <xsl:template match="JWM">
      <xsl:copy>
         <xsl:apply-templates select="node()|@*"/>

         <!-- Add TaskListStyle if it is missing. -->
         <xsl:if test="not(section/@name='xyz')">
            <TaskListStyle>
               <xsl:attribute name="list">
                  <xsl:value-of select="//TrayStyle/@list"/>
               </xsl:attribute>
               <xsl:attribute name="group">
                  <xsl:value-of select="//TrayStyle/@group"/>
               </xsl:attribute>
            </TaskListStyle>
         </xsl:if>

         <!-- Insert default mouse bindings. -->
         <Mouse context="root" button="4">ldesktop</Mouse>
         <Mouse context="root" button="5">rdesktop</Mouse>
         <Mouse context="title" button="1">move</Mouse>
         <Mouse context="title" button="2">move</Mouse>
         <Mouse context="title" button="3">window</Mouse>
         <Mouse context="title" button="4">shade</Mouse>
         <Mouse context="title" button="5">shade</Mouse>
         <Mouse context="title" button="11">maximize</Mouse>
         <Mouse context="icon" button="1">window</Mouse>
         <Mouse context="icon" button="2">move</Mouse>
         <Mouse context="icon" button="3">window</Mouse>
         <Mouse context="icon" button="4">shade</Mouse>
         <Mouse context="icon" button="5">shade</Mouse>
         <Mouse context="border" button="1">resize</Mouse>
         <Mouse context="border" button="2">move</Mouse>
         <Mouse context="border" button="3">window</Mouse>
         <Mouse context="close" button="-1">close</Mouse>
         <Mouse context="close" button="2">move</Mouse>
         <Mouse context="close" button="-3">close</Mouse>
         <Mouse context="maximize" button="-1">maximize</Mouse>
         <Mouse context="maximize" button="-2">maxv</Mouse>
         <Mouse context="maximize" button="-3">maxh</Mouse>
         <Mouse context="minimize" button="-1">minimize</Mouse>
         <Mouse context="minimize" button="2">move</Mouse>
         <Mouse context="minimize" button="-3">shade</Mouse>
      </xsl:copy>
   </xsl:template>

   <!-- Move the group/list attributes to TaskListStyle. -->
   <xsl:template match="TaskListStyle">
      <xsl:copy>
         <xsl:attribute name="list">
            <xsl:value-of select="//TrayStyle/@list"/>
         </xsl:attribute>
         <xsl:attribute name="group">
            <xsl:value-of select="//TrayStyle/@group"/>
         </xsl:attribute>
         <xsl:apply-templates select="@*|node()"/>
      </xsl:copy>
   </xsl:template>

</xsl:stylesheet>
