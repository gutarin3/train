#!/usr/local/bin/ruby
require "xmlrpc/client"
require "rss"

rss = RSS::Parser.parse('http://b.hatena.ne.jp/hotentry?mode=rss')

body =""

rss.items.each do |item|
body = body + "<a href="&quot; + item.link + &quot;">" + item.title + "</a>"
end

content={"title" =&gt; "Post entry into WordPress via ruby", "description" =&gt; body }
server = XMLRPC::Client.new("サーバ名","/wp/xmlrpc.php")
server.call('metaWeblog.newPost','1','ユーザ名','パスワード',content,true)
