Option Explicit

Dim mainDoc, includeDoc
Dim providers, provider, templates, template
Dim index
Dim name, namespace
Dim parents, parent, node, appendTo, firstChild, prev
Dim languages, language
Dim stringTables, stringTable, appendToStringTable
Dim regex, providerMessageId, channelMessageId, providerMessageNode, channelMessageNode

Set mainDoc = CreateObject("MSXML2.DOMDocument")
mainDoc.load(WScript.Arguments(0))

Set providers = mainDoc.selectNodes("/instrumentationManifest/instrumentation/events/provider")
For index = WScript.Arguments.Count - 1 To 2 Step -1
	Set includeDoc = CreateObject("MSXML2.DOMDocument")
	includeDoc.load(WScript.Arguments(index))

	Set prev = Nothing
	For Each provider In providers
		For Each name In Array("channels", "levels", "tasks", "opcodes", "keywords", "filters", "maps", "templates", "events")
			Set appendTo = provider.selectSingleNode(name)
			Set parents = includeDoc.selectNodes("/instrumentationManifest/instrumentation/events/provider/" & name)
			For Each parent In parents
				If parent.childNodes.Length > 0 Then
					If name = "channels" And provider.getAttribute("name") <> "_include" Then
						For Each node In parent.childNodes
							node.setAttribute "name", provider.getAttribute("name") & "/" & node.getAttribute("name")
							Set regex = New RegExp
							regex.Pattern = "^\$\([Ss]tring.([^)]+)\)$"
							providerMessageId = regex.Replace(provider.getAttribute("message"), "$1")
							channelMessageId = regex.Replace(node.getAttribute("message"), "$1")

							' Fix channel names
							Set languages = mainDoc.selectNodes("/instrumentationManifest/localization/resources")
							For Each language In languages
								Set providerMessageNode = language.selectSingleNode("stringTable/string[@id=""" & providerMessageId & """]")
								Set channelMessageNode = includeDoc.selectSingleNode("/instrumentationManifest/localization/resources[@culture=""" & language.getAttribute("culture") & """]/stringTable/string[@id=""" & channelMessageId & """]")

								channelMessageNode.setAttribute "value", providerMessageNode.getAttribute("value") & "/" & channelMessageNode.getAttribute("value")
							Next
						Next
					End If
					If appendTo Is Nothing Then
						If prev Is Nothing Then
							Set appendTo = provider.insertBefore(parent, provider.firstChild)
						Else
							Set appendTo = provider.insertBefore(parent, prev.nextSibling)
						End If
					Else
						Set firstChild = appendTo.firstChild
						For Each node In parent.childNodes
							appendTo.insertBefore node, firstChild
						Next
					End If
					Set prev = appendTo
				End If
			Next
			If Not appendTo Is Nothing Then
				Set prev = appendTo
			End If
		Next
	Next

	Set languages = includeDoc.selectNodes("/instrumentationManifest/localization/resources")
	For Each language In languages
		Set stringTables = language.selectNodes("stringTable")
		For Each stringTable In stringTables
			If stringTable.childNodes.Length > 0 Then
				Set appendTo = mainDoc.selectSingleNode("/instrumentationManifest/localization/resources[@culture=""" & language.getAttribute("culture") & """]")
				If appendTo Is Nothing Then
					Set appendTo = mainDoc.selectSingleNode("/instrumentationManifest/localization")
					If appendTo Is Nothing Then
						Set appendTo = mainDoc.selectSingleNode("instrumentationManifest").appendChild(includeDoc.selectSingleNode("/instrumentationManifest/localization").cloneNode(False))
					End If
					appendTo.insertBefore language, appendTo.firstChild
				Else
					appendTo.insertBefore stringTable, appendTo.firstChild
				End If
			End If
		Next
	Next
Next

Set templates = mainDoc.selectNodes("/instrumentationManifest/instrumentation/events/provider/templates/template")
For Each template In templates
	Set node = mainDoc.createNode(1, "data", template.namespaceURI)
	node.setAttribute "name", "source"
	node.setAttribute "inType", "win:AnsiString"
	node.setAttribute "outType", "xs:string"
	template.appendChild node

	Set node = mainDoc.createTextNode(vbNewLine & String(6, vbTab))
	template.appendChild node

	Set node = mainDoc.createNode(1, "data", template.namespaceURI)
	node.setAttribute "name", "line"
	node.setAttribute "inType", "win:UInt32"
	node.setAttribute "outType", "xs:unsignedInt"
	template.appendChild node

	Set node = mainDoc.createTextNode(vbNewLine & String(5, vbTab))
	template.appendChild node
Next

mainDoc.save(WScript.Arguments(1))
