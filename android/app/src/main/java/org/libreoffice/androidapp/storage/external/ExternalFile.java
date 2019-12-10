package org.libreoffice.androidapp.storage.external;

import android.content.Context;
import android.util.Log;

import org.libreoffice.androidapp.storage.IFile;
import org.libreoffice.androidapp.storage.IOUtils;

import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import androidx.documentfile.provider.DocumentFile;

/**
 * Implementation of IFile for the external file system, for Android 4.4+
 *
 * Uses the DocumentFile class.
 *
 * The DocumentFile class obfuscates the path of the files it wraps,
 * preventing usage of LOK's documentLoad method. A copy of the DocumentFile's contents
 * will be created in the cache when files are opened, allowing use of documentLoad.
 */
public class ExternalFile implements IFile{
    private final static String LOGTAG = "ExternalFile";

    private ExtsdDocumentsProvider provider;
    private DocumentFile docFile;

    /** We create the document just once, cache it for further returning. */
    private File mCachedFile;

    private Context context;

    public ExternalFile(ExtsdDocumentsProvider provider, DocumentFile docFile, Context context) {
        this.provider = provider;
        this.context = context;
        this.docFile = docFile;
    }

    @Override
    public URI getUri() {
        try{
            return new URI(docFile.toString());
        } catch (URISyntaxException e) {
            Log.e(LOGTAG, e.getMessage(), e.getCause());
            return null;
        }
    }

    @Override
    public String getName() {
        return docFile.getName();
    }

    @Override
    public boolean isDirectory() {
        return docFile.isDirectory();
    }

    @Override
    public long getSize() {
        return docFile.length();
    }

    @Override
    public Date getLastModified() {
        return new Date(docFile.lastModified());
    }

    @Override
    public List<IFile> listFiles() {
        List<IFile> children = new ArrayList<IFile>();
        for (DocumentFile child : docFile.listFiles()) {
            children.add(new ExternalFile(provider, child, context));
        }
        return children;
    }

    @Override
    public List<IFile> listFiles(FileFilter filter) {
        File file;
        try{
            List<IFile> children = new ArrayList<IFile>();
            for (DocumentFile child : docFile.listFiles()) {
                file = new File(new URI(child.getUri().toString()));
                if(filter.accept(file))
                    children.add(new ExternalFile(provider, child, context));
            }
            return children;

        }catch (Exception e){
            e.printStackTrace();
        }
        /* if something goes wrong */
        return listFiles();

    }

    @Override
    public IFile getParent(Context context) {
        // this is the root node
        if(docFile.getParentFile() == null) return null;

        return new ExternalFile(provider, docFile.getParentFile(), this.context);
    }

    @Override
    public File getDocument() {
        if (mCachedFile != null)
            return mCachedFile;

        if (isDirectory())
            return null;

        mCachedFile = duplicateInCache();
        return mCachedFile;
    }

    private File duplicateInCache() {
        try{
            InputStream istream = context.getContentResolver().
                    openInputStream(docFile.getUri());

            File storageFolder = provider.getCacheDir();
            File fileCopy = new File(storageFolder, docFile.getName());
            OutputStream ostream = new FileOutputStream(fileCopy);

            IOUtils.copy(istream, ostream);
            return fileCopy;
        } catch (Exception e) {
            Log.e(LOGTAG, e.getMessage(), e.getCause());
            return null;
        }
    }

    @Override
    public void saveDocument() {
        if (mCachedFile == null) {
            Log.e(LOGTAG, "Trying to save document that was not created via getDocument()(");
            return;
        }

        try{
            OutputStream ostream = context.getContentResolver().
                    openOutputStream(docFile.getUri());
            InputStream istream = new FileInputStream(mCachedFile);

            IOUtils.copy(istream, ostream);

        } catch (Exception e) {
            Log.e(LOGTAG, e.getMessage(), e.getCause());
        }
    }

    @Override
    public boolean equals(Object object) {
        if (this == object)
            return true;
        if (!(object instanceof ExternalFile))
            return false;
        ExternalFile file = (ExternalFile) object;
        return file.getUri().equals(getUri());
    }

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
