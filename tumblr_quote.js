<script type=”text/javascript” src=”http://you.tumblr.com/api/read/json?num=1&type=quote”
></script>
<script type=”text/javascript” >
// 表示設定部分 ———————————————-
//// 表示個数を変える場合はここと上の JSON URL の num 部分を変更
show_number = 1;
// テキストを途中でカットする文字数
text_cut = 100;
// ———————————————-

for (i = 0; i < show_number; i++) {
text = tumblr_api_read["posts"][i]["quote-text"];
source= tumblr_api_read["posts"][i]["quote-source"];

// HTML タグを全て剥ぎ取る
text = text.replace(/<[^>]*?>/gi,””);

// テキストを設定した文字数でカット
if (text.length > text_cut) text = text.substring(0,text_cut) + “…”;

url = tumblr_api_read["posts"][i]["url"];
document.write(‘<font size=”-2″><a href=”‘, url , ‘”>[', i+1, ']</a> ‘, text, ‘<br />’, source ,’</font><br /><br />’);}</script>
