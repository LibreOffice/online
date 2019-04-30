/* -*- js-indent-level: 8 -*- */

/* global vex _ getParameterByName */
var errorMessages = {};

vex.defaultOptions.className = 'vex-theme-plain';

var lang = getParameterByName('lang');
if (lang) {
	String.locale = lang;
}

errorMessages.diskfull = _('No disk space left on server, please contact the server administrator to continue.');
errorMessages.emptyhosturl = _('The host URL is empty. The loolwsd server is probably misconfigured, please contact the administrator.');
errorMessages.limitreached = _('This is an unsupported version of {productname}. To avoid the impression that it is suitable for deployment in enterprises, this message appears when more than {docs} documents or {connections} connections are in use concurrently');
errorMessages.infoandsupport = _('More information and support');
errorMessages.limitreachedprod = _('This service is limited to %0 documents, and %1 connections total by the admin. This limit has been reached. Please try again later.');
errorMessages.serviceunavailable = _('Service is unavailable. Please try again later and report to your administrator if the issue persists.');
errorMessages.unauthorized = _('Unauthorized WOPI host. Please try again later and report to your administrator if the issue persists.');
errorMessages.wrongwopisrc = _('Wrong or missing WOPISrc parameter, please contact support.');
errorMessages.sessionexpiry = _('Your session will expire in %time. Please save your work and refresh the session (or webpage) to continue.');
errorMessages.sessionexpired = _('Your session has been expired. Further changes to document might not be saved. Please refresh the session (or webpage) to continue.');
errorMessages.faileddocloading = _('Failed to load the document. Please ensure the file type is supported and not corrupted, and try again.');

errorMessages.storage = {
	loadfailed: _('Failed to read document from storage. Please contact your storage server (%storageserver) administrator.'),
	savediskfull: _('Save failed due to no disk space left on storage server. Document will now be read-only. Please contact the server (%storageserver) administrator to continue editing.'),
	saveunauthorized: _('Document cannot be saved due to expired or invalid access token.'),
	savefailed: _('Document cannot be saved. Check your permissions or contact the storage server administrator.'),
	renamefailed: _('Document cannot be renamed. Check your permissions or contact the storage server administrator.')
};

if (typeof window !== 'undefined') {
	window.errorMessages = errorMessages;
}
