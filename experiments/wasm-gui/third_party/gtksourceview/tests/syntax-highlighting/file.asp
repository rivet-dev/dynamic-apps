<html>
<body>

<%
dim d
set d=Server.CreateObject("Scripting.Dictionary")
d.Add "o", "Orange"
d.Add "a", "Apple"
if d.Exists("o")= true then
    Response.Write("Key exists.")
else
    Response.Write("Key does not exist.")
end if
set d=nothing
%>

</body>
</html>
