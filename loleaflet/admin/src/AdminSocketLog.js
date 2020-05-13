/* -*- js-indent-level: 8 -*- */
/*
	Socket to be intialized on opening the log page in Admin console
*/
/* global Admin $ AdminSocketBase */
var AdminSocketLog = AdminSocketBase.extend({
	constructor: function(host) {
		this.base(host);
	},

	refreshLog: function() {
		this.socket.send('log_lines');
	},

	onSocketOpen: function() {
		// Base class' onSocketOpen handles authentication
		this.base.call(this);

		var socketLog = this;
		$('#refresh-log').on('click', function () {
			return socketLog.refreshLog();
		});
		this.refreshLog();
	},

	onSocketMessage: function(e) {
		if (e.data.startsWith('log_lines')) {
			var result = e.data;
			result = result.substring(10, result.length);
			document.getElementById('log-lines').value = result;
		}
	},

	onSocketClose: function() {

	}
});

Admin.Log = function(host) {
	return new AdminSocketLog(host);
};
