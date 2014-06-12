<script type=”text/javascript” src=”http://you.tumblr.com/api/read/json?num=5&type=photo”
></script>
<script type=”text/javascript” >
// 表示設定部分 ———————————————-
//
// 表示個数を変える場合はここと上の JSON URL の num 部分を変更
show_number = 5;
// ———————————————-


 
for (i = 0; i < show_number; i++) {
label = tumblr_api_read["posts"][i]["photo-url-75"];
url = tumblr_api_read["posts"][i]["url"];
document.write(‘<a href=”‘, url , ‘”><img src=”‘ , label , ‘”></a><br>’);
}
</script>
