/* -*- js-indent-level: 8 -*- */
/*
	Socket to be intialized on opening the log page in Admin console
*/
/* global Admin $ AdminSocketBase */
var AdminSocketLog = AdminSocketBase.extend({
	constructor: function(host) {
		this.base(host);
		// There is a "$" is never used error. Let's get rid of this. This is vanilla script and has not more lines than the one with JQuery.
		$('#form-channel-list').id;
	},

	refreshLog: function() {
		this.socket.send('log_lines');
	},

	pullChannelList: function() {
		this.socket.send('channel_list');
	},

	sendChannelListLogLevels: function(e) {
		e.stopPropagation();

		// We change the colour of the button when we send the data and change it back when the task is done (in function applyChannelList). But it is happening too fast.
		document.getElementById('update-log-levels').classList.add('is-warning');
		document.getElementById('update-log-levels').classList.remove('is-info');

		// Get the form.
		var form = document.getElementById('form-channel-list');

		// Get channel select elements.
		var selectList = form.querySelectorAll('select');

		// Prepare the statement.
		var textToSend = 'update-log-levels';
		for (var i = 0; i < selectList.length; i++) {
			textToSend += ' ' + selectList[i].getAttribute('name').replace('channel-', '') + '=' + selectList[i].value;
		}

		this.socket.send(textToSend);
		document.getElementById('channel-list-modal').classList.remove('is-active');
	},

	onSocketOpen: function() {
		// Base class' onSocketOpen handles authentication
		this.base.call(this);

		document.getElementById('refresh-log').onclick = this.refreshLog.bind(this);
		document.getElementById('update-log-levels').onclick = this.sendChannelListLogLevels.bind(this);

		this.pullChannelList();
		this.refreshLog();
	},

	applyChannelList: function(channelListStr) {
		var channelListArr = channelListStr.split(' '); // Every item holds: channel name + = + log level.

		// Here we have the log channel list and their respective log levels.
		// We will create items for them. User will be able to set the log level for each channel.
		var channelForm = document.getElementById('form-channel-list');
		channelForm.innerHTML = ''; // Clear and refill it.
		var optionList = Array('none', 'fatal', 'critical', 'error', 'warning', 'notice', 'information', 'debug', 'trace');
		var innerHTML = ''; // Of select elements.
		for (var i = 0; i < optionList.length; i++) {
			innerHTML += '<option value="' + optionList[i] + '">' + optionList[i] + '</option>';
		}

		for (i = 0; i < channelListArr.length; i++) {
			if (channelListArr[i].split('=').length === 2) {
				var channelName = channelListArr[i].split('=')[0];
				var channelLogLevel = channelListArr[i].split('=')[1];

				var newDiv = document.createElement('div');
				newDiv.className = 'content';

				var newLabel = document.createElement('label');
				newLabel.className = 'label is-normal';
				newLabel.setAttribute('for', 'channel-' + channelName);
				newLabel.innerText = channelName;

				var newSubDivision = document.createElement('div');
				newSubDivision.className = 'select';

				var newSelectElement = document.createElement('select');
				newSelectElement.name = 'channel-' + channelName;
				newSelectElement.id = 'channel-' + channelName;
				newSelectElement.innerHTML = innerHTML;
				newSelectElement.value = channelLogLevel;
				newSelectElement.style.width = '160px';
				newSelectElement.className = 'form-control';

				channelForm.appendChild(newDiv);
				newDiv.appendChild(newLabel);
				newDiv.appendChild(newSubDivision);
				newSubDivision.appendChild(newSelectElement);
			}
		}

		document.getElementById('update-log-levels').classList.remove('is-warning');
		document.getElementById('update-log-levels').classList.add('is-info');
	},

	onSocketMessage: function(e) {
		if (e.data.startsWith('log_lines')) {
			var result = e.data;
			result = result.substring(10, result.length);
			document.getElementById('log-lines').value = result;
		}
		else if (e.data.startsWith('channel_list')) {
			var channelListStr = e.data.substring(13, e.data.length);
			this.applyChannelList(channelListStr);
		}
	},

	onSocketClose: function() {

	}
});

Admin.Log = function(host) {
	return new AdminSocketLog(host);
};
