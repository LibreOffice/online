/* eslint-disable */
/// Available types: info, warning, danger, link, success, primary. Works with bulma.css.
// Every "set" function returns the instance. So you can do this:
// (new DlgYesNo).Title('some title').Text('some text').YesButtonText('yes').NoButtonText('no').YesFunction(function () {/* */}).NoFunction(function() {/** */});
// "Yes" and "No" buttons call callback function, close the modal and destroy the modal.
var DlgYesNo = /** @class */ (function () {
    function DlgYesNo() {
        this.instance = this;
        DlgYesNo.instanceCount++;
        this.modalID = DlgYesNo.instanceCount;
        this.__Initialize();
    }
    DlgYesNo.prototype.__Initialize = function () {
        var html = this.__GetModalHTML();
        var element = document.createElement('div');
        element.innerHTML = html;
        document.getElementsByTagName('body')[0].appendChild(element);
        this.__InitializeBackgroundClick();
        this.__InitializeCrossButton();
        this.__InitializeYesButton();
        this.__InitializeNoButton();
    };
    DlgYesNo.prototype.__InitializeCrossButton = function () {
        var element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-cross-button-' + String(this.modalID)).onclick = function () {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    };
    DlgYesNo.prototype.__InitializeBackgroundClick = function () {
        var element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-background-' + String(this.modalID)).onclick = function () {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    };
    DlgYesNo.prototype.__InitializeYesButton = function () {
        var element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-yes-button-' + String(this.modalID)).onclick = function () {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    };
    DlgYesNo.prototype.__InitializeNoButton = function () {
        var element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-no-button-' + String(this.modalID)).onclick = function () {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    };
    DlgYesNo.prototype.__GetModalHTML = function () {
        var html = ' \
<div class="modal" id="modal-__created_id__"> \
    <div class="modal-background" id="modal-background-__created_id__"></div> \
    <div class="modal-card"> \
        <header class="modal-card-head" id="modal-head-__created_id__"> \
            <p class="modal-card-title" id="modal-title-__created_id__">Yes / No Modal Template</p> \
            <button class="delete" id="modal-cross-button-__created_id__"></button> \
        </header> \
        <section class="modal-card-body" id="modal-body-__created_id__">Yes / No Modal Body</section> \
        <footer class="modal-card-foot is-fullwidth" id="modal-foot-__created_id__"> \
            <button type="button" class="button is-pulled-left" id="modal-no-button-__created_id__" style="min-width:120px;">Cancel</button> \
            <button type="button" class="button is-pulled-right" id="modal-yes-button-__created_id__" style="min-width:120px;">OK</button> \
        </footer> \
    </div> \
</div>';
        html = html.split('__created_id__').join(String(this.modalID));
        return html;
    };
    DlgYesNo.prototype.YesButtonText = function (text) {
        var button = document.getElementById('modal-yes-button-' + String(this.modalID));
        button.innerText = text;
        return this.instance;
    };
    DlgYesNo.prototype.NoButtonText = function (text) {
        var button = document.getElementById('modal-no-button-' + String(this.modalID));
        button.innerText = text;
        return this.instance;
    };
    DlgYesNo.prototype.Title = function (text) {
        var p = document.getElementById('modal-title-' + String(this.modalID));
        p.innerText = text;
        return this.instance;
    };
    DlgYesNo.prototype.Text = function (text) {
        var d = document.getElementById('modal-body-' + String(this.modalID));
        d.innerText = text;
        return this.instance;
    };
    DlgYesNo.prototype.Type = function (type) {
        var header = document.getElementById('modal-head-' + String(this.modalID));
        header.className = 'modal-card-head has-background-' + type;
        return this.instance;
    };
    DlgYesNo.prototype.YesFunction = function (f) {
        var element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-yes-button-' + String(this.modalID)).onclick = f, function () {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
        return this.instance;
    };
    DlgYesNo.prototype.NoFunction = function (f) {
        var element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-no-button-' + String(this.modalID)).onclick = f, function () {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
        return this.instance;
    };
    DlgYesNo.prototype.Open = function () {
        document.getElementById('modal-' + String(this.modalID)).classList.add('is-active');
    };
    DlgYesNo.instanceCount = 0;
    return DlgYesNo;
}());
