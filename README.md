<div class="mandoc">
<table class="head">
<tbody>
<tr>
<td class="head-ltitle">
RANDRD(1)</td>
<td class="head-vol">
General Commands Manual</td>
<td class="head-rtitle">
RANDRD(1)</td>
</tr>
</tbody>
</table>
<div class="section">
<h1 id="x4e414d45">NAME</h1> <b class="name">randrd</b> &#8212; <span class="desc">randr monitor daemon</span></div>
<div class="section">
<h1 id="x53594e4f50534953">SYNOPSIS</h1><table class="synopsis">
<col style="width: 6.00ex;"/>
<col/>
<tbody>
<tr>
<td>
randrd</td>
<td>
&#91;<span class="opt"><b class="flag">&#45;d</b></span>&#93; &#91;<span class="opt"><b class="flag">&#45;f</b> <i class="arg">file</i></span>&#93; &#91;<span class="opt"><b class="flag">&#45;i</b> <i class="arg">interval</i></span>&#93;</td>
</tr>
</tbody>
</table>
<br/>
<table class="synopsis">
<col style="width: 6.00ex;"/>
<col/>
<tbody>
<tr>
<td>
randrd</td>
<td>
<b class="flag">&#45;E</b></td>
</tr>
</tbody>
</table>
</div>
<div class="section">
<h1 id="x4445534352495054494f4e">DESCRIPTION</h1> The <a class="link-man">Xorg(1)</a> server relies on libudev to monitor/detect physical screen configuration changes.  Interested partys can register the <span class="symb">XRRScreenChangeNotify</span> event via <a class="link-man">XRRSelectInput(3)</a> to get notified. Systems that lack libudev will not get any <span class="symb">XRRScreenChangeNotify</span> events when the physical screen configuration changes.<div class="spacer">
</div>
The <b class="name">randrd</b> utility polls screen resources from Xrandr(3) after a regular <i class="arg">interval</i>. This causes <a class="link-man">Xorg(1)</a> to check if any screen configuration has changed and will emit the appropriate events which will be handled by <b class="name">randrd</b>. If the physical screen configuration has changed, <b class="name">randrd</b> will execute the specified <i class="arg">file</i> and passes the <span class="symb">CONNECTIONSTATE</span>, the <span class="symb">OUTPUT</span> which has changed and an <a class="link-man">rmd160(3)</a> <span class="symb">EDIDHASH</span> over all screen EDID's. <b class="name">randrd</b> supports the following flags:<dl style="margin-top: 0.00em;margin-bottom: 0.00em;" class="list list-tag">
<dt class="list-tag" style="margin-top: 1.00em;">
<b class="flag">&#45;d</b></dt>
<dd class="list-tag" style="margin-left: 0.50ex;">
Debug mode.  Don't detach or become a daemon.</dd>
<dt class="list-tag" style="margin-top: 1.00em;">
<b class="flag">&#45;f</b> <i class="arg">file</i></dt>
<dd class="list-tag" style="margin-left: 0.50ex;">
Specifies the file which will be executed if the physical screen configuration has changed.  The default is <i class="file">$HOME/.randrd</i>.</dd>
<dt class="list-tag" style="margin-top: 1.00em;">
<b class="flag">&#45;i</b> <i class="arg">interval</i></dt>
<dd class="list-tag" style="margin-left: 0.50ex;">
Specifies the default interval in seconds after which screen resources are polled.  The allowed range is between 1 and 60.  The default is 3 seconds.</dd>
<dt class="list-tag" style="margin-top: 1.00em;">
<b class="flag">&#45;E</b></dt>
<dd class="list-tag" style="margin-left: 0.50ex;">
Prints the <a class="link-man">rmd160(3)</a> <span class="symb">EDIDHASH</span> for the actual screen configuration to stdout.</dd>
</dl>
</div>
<div class="section">
<h1 id="x46494c4553">FILES</h1><dl style="margin-top: 0.00em;margin-bottom: 0.00em;" class="list list-tag">
<dt class="list-tag" style="margin-top: 0.00em;">
$HOME/.randrd</dt>
<dd class="list-tag" style="margin-left: 0.54ex;">
default <i class="arg">file</i> to execute when outputs change.</dd>
</dl>
</div>
<div class="section">
<h1 id="x4558414d504c4553">EXAMPLES</h1> Sample <i class="file">.randrd</i> script:<div class="spacer">
</div>
<pre style="margin-left: 5.00ex;" class="lit display">
#!/bin/sh 
 
CONNECTIONSTATE=$1 
OUTPUT=$2 
EDIDHASH=$3 
 
case $EDIDHASH in 
&quot;9030ffc857dc906a635d1da0799b22d25e2b814e&quot;) 
	CUR=&quot;Work&quot; 
	xrandr --output VGA1 --auto --above LVDS1 
	;; 
&quot;67034f8cfb71cebf10c9bc21dd9eac9f06512861&quot;) 
	CUR=&quot;Home&quot; 
	xrandr --output VAG1 --auto --right-of LVDS1 
	;; 
*) 
	CUR=&quot;Default&quot; 
	xrandr --auto 
	;; 
esac 
 
logger &quot;$OUTPUT $CONNECTIONSTATE: Loading $CUR xrandr profile&quot;</pre>
</div>
<div class="section">
<h1 id="x53454520414c534f">SEE ALSO</h1> <a class="link-man">xrandr(1)</a> <a class="link-man">Xrandr(3)</a></div>
<div class="section">
<h1 id="x415554484f5253">AUTHORS</h1> The <b class="name">randrd</b> program was written by <span class="author">Fabian Raetz</span> &#60;<a class="link-mail" href="mailto:fabian.raetz@gmail.com">fabian.raetz@gmail.com</a>&#62;.</div>
<div class="section">
<h1 id="x43415645415453">CAVEATS</h1> Constantly polling screen resources from <a class="link-man">Xrandr(3)</a> can decrease performance. If your System has libudev support, consider using an alternative utility like <a class="link-man">srandrd(1)</a></div>
<table class="foot">
<tbody>
<tr>
<td class="foot-date">
October 13, 2014</td>
<td class="foot-os">
OpenBSD 5.6</td>
</tr>
</tbody>
</table>
</div>

