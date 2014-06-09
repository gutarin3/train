import urllib2
fp = urllib2.urlopen('http://gkd-planning.jp/')
html = fp.read()
print (html)
fp.close()
