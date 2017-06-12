/*
	Socket to be intialized on opening the history page in Admin console
*/
/* global $ nodejson2html Util AdminSocketBase */
/* eslint no-unused-vars:0 */
var AdminSocketHistory = AdminSocketBase.extend({
	constructor: function(host) {
		this.base(host);
	},

	onSocketOpen: function() {
		// Base class' onSocketOpen handles authentication
		this.base.call(this);
		this.socket.send('history');
		this.socket.send('subscribe {"History":');
	},

	onSocketMessage: function(e) {
		//if (e.data == 'InvalidAuthToken' || e.data == 'NotAuthenticated') {
		//	this.onSocketOpen();
		//} else {
		var jsonObj;
		try {
			jsonObj = JSON.parse(e.data);
			var doc = jsonObj['History']['documents'];
			var exdoc = jsonObj['History']['expiredDocuments'];
			$('#json-doc').find('textarea').html(JSON.stringify(doc));
			$('#json-ex-doc').find('textarea').html(JSON.stringify(exdoc));
		} catch (e) {
			$('document').alert(e.message);
		}
	},

	onSocketClose: function() {
		this.socket.send('unsubscribe {"History":');
	}
});

Admin.History = function(host) {
	return new AdminSocketHistory(host);
};

