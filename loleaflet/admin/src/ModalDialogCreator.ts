/* eslint-disable */

/// Available types: info, warning, danger, link, success, primary. Works with bulma.css.

// Every "set" function returns the instance. So you can do this:
// (new DlgYesNo).Title('some title').Text('some text').YesButtonText('yes').NoButtonText('no').YesFunction(function () {/* */}).NoFunction(function() {/** */});
// "Yes" and "No" buttons call callback function, close the modal and destroy the modal.

class DlgYesNo {
    static instanceCount: number = 0;
    instance: DlgYesNo;
    modalID: number;

    constructor() {
        this.instance = this;
        DlgYesNo.instanceCount++;
        this.modalID = DlgYesNo.instanceCount;
        this.__Initialize();
    }

    __Initialize() {
        let html: string = this.__GetModalHTML();
        let element: HTMLDivElement = document.createElement('div');
        element.innerHTML = html;
        document.getElementsByTagName('body')[0].appendChild(element);
        this.__InitializeBackgroundClick();
        this.__InitializeCrossButton();
        this.__InitializeYesButton();        
        this.__InitializeNoButton();
    }

    __InitializeCrossButton() {
        let element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-cross-button-' + String(this.modalID)).onclick = function() {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    }

    __InitializeBackgroundClick() {
        let element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-background-' + String(this.modalID)).onclick = function() {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    }

    __InitializeYesButton() {
        let element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-yes-button-' + String(this.modalID)).onclick = function() {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    }

    __InitializeNoButton() {
        let element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-no-button-' + String(this.modalID)).onclick = function() {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
    }

    __GetModalHTML(): string {
        let html: string = ' \
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
    }

    YesButtonText(text: string): DlgYesNo {
        let button: HTMLButtonElement = <HTMLButtonElement>document.getElementById('modal-yes-button-' + String(this.modalID));
        button.innerText = text;
        return this.instance;
    }

    NoButtonText(text: string): DlgYesNo {
        let button: HTMLButtonElement = <HTMLButtonElement>document.getElementById('modal-no-button-' + String(this.modalID));
        button.innerText = text;
        return this.instance;
    }

    Title(text: string): DlgYesNo {
        let p: HTMLParagraphElement = <HTMLParagraphElement>document.getElementById('modal-title-' + String(this.modalID));
        p.innerText = text;
        return this.instance;
    }
    
    Text(text: string): DlgYesNo {
        let d: HTMLDivElement = <HTMLDivElement>document.getElementById('modal-body-' + String(this.modalID));
        d.innerText = text;
        return this.instance;
    }

    Type(type: string): DlgYesNo {
        let header: HTMLDivElement = <HTMLDivElement>document.getElementById('modal-head-' + String(this.modalID));
        header.className = 'modal-card-head has-background-' + type;
        return this.instance;
    }

    YesFunction(f: any): DlgYesNo {
        let element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-yes-button-' + String(this.modalID)).onclick = f, function() {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
        return this.instance;
    }

    NoFunction(f: any): DlgYesNo {
        let element = document.getElementById('modal-' + String(this.modalID));
        document.getElementById('modal-no-button-' + String(this.modalID)).onclick = f, function() {
            element.classList.remove('is-active');
            element.parentNode.removeChild(element);
        };
        return this.instance;
    }

    Open() {
        document.getElementById('modal-' + String(this.modalID)).classList.add('is-active');
    }
}