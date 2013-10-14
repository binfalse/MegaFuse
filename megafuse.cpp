#include "mega.h"

#include "megacrypto.h"
#include "megaclient.h"
#include "megafuse.h"

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

void MegaFuse::event_loop(MegaFuse* that)
{
    char *saved_line = NULL;
    int saved_point = 0;

	for (;;)
	{
	    that->engine_mutex.lock();
		that->client->exec();
		that->engine_mutex.unlock();
		that->client->wait();
	}
}

bool MegaFuse::start()
{
    event_loop_thread = std::thread(event_loop,this);
}

bool MegaFuse::login(std::string username, std::string password)
{
    std::lock_guard<std::mutex>lock(api_mutex);
    {

        engine_mutex.lock();
        client->pw_key(password.c_str(),pwkey);
        client->login(username.c_str(),pwkey);
        login_ret=0;
        engine_mutex.unlock();
        printf("login: aspetto il risultato\n");
        std::unique_lock<std::mutex> lk(cvm);
        cv.wait(lk, [this]{return login_ret;});
    }

    if(login_ret < 0)
        return false;
    else
        return true;
}

std::vector<std::string> MegaFuse::ls(std::string path)
{
    /*std::cout << path <<std::endl;
       engine_mutex.lock();

    Node* n = nodeByPath(path);
    if(!n)
    {
        engine_mutex.unlock();
        return std::vector<std::string>();
    }

   std::vector<std::string> l;
   if(n->type == FOLDERNODE || n->type == ROOTNODE)
		{
            for (node_list::iterator it = n->children.begin(); it != n->children.end(); it++)
            l.push_back( (*it)->displayname());
		}
   engine_mutex.unlock();
    return l;*/
}

file_stat MegaFuse::getAttr(std::string path)
{
  /*  std::lock_guard<std::mutex>lock(api_mutex);
    engine_mutex.lock();
    std::cout <<"PATH: "<<path<<std::endl;
    Node *n = nodeByPath(path);
    if(!n)
    {
        engine_mutex.unlock();
        file_stat fs;
        fs.error = -ENOENT;
        return fs;
    }
    file_stat fs;
    switch (n->type)
    {
			case FILENODE:
                fs.mode = 0444;
                fs.nlink = 1;
                fs.size = n->size;
                fs.dir = false;
                fs.mtime = n->mtime;
                break;

			case FOLDERNODE:
			case ROOTNODE:
				fs.mode = 0755;
                fs.nlink = 1;
                fs.size = 0;
                fs.dir = true;
                fs.mtime = n->mtime;
				break;
    }
    engine_mutex.unlock();
		return fs;*/

}

std::pair<std::string,std::string> MegaFuse::splitPath(std::string path)
{
    int pos = path.find_last_of('/');
    std::string basename = path.substr(0,pos);
    std::string filename = path.substr(pos+1);
    if(basename == "")
       basename = "/";
    return std::pair<std::string,std::string> (basename,filename);
}

//warning, no mutex
Node* MegaFuse::nodeByPath(std::string path)
{
    if(engine_mutex.try_lock())
     {
         printf("errore mutex in nodebypath\n");
         abort();
     }
    if (path == "/")
        return client->nodebyhandle(client->rootnodes[0]);
    if(path[0] == '/')
        path = path.substr(1);
    Node *n = client->nodebyhandle(client->rootnodes[0]);

    int pos;
    while ((pos = path.find_first_of('/')) > 0)
    {

        printf("analizzo la stringa %s, pos = %d\n",path.c_str(),pos);
        n = childNodeByName(n,path.substr(0,pos));
        if(!n)
            return NULL;
        path = path.substr(pos+1);
    }
    n = childNodeByName(n,path);
        if(!n)
            return NULL;
    printf("nodo trovato\n");
    return n;
}

Node* MegaFuse::childNodeByName(Node *p,std::string name)
{
    if(engine_mutex.try_lock())
      {
          abort();
      }
    for (node_list::iterator it = p->children.begin(); it != p->children.end(); it++)
    {
        if (!strcmp(name.c_str(),(*it)->displayname()))
        {
            printf("confronto riuscito\n");
            return *it;
        }
    }
    printf("confronto fallito\n");
    return NULL;
}

bool MegaFuse::upload(std::string filename,std::string dst)
{
   /* std::lock_guard<std::mutex>lock(api_mutex);
    engine_mutex.lock();


    int pos = dst.find_last_of('/');
    std::string base = dst.substr(0,pos);
    std::string dstname = dst.substr(pos+1);
    if(base == "")
        base = "/";
    if(dstname == "")
        dstname = filename;

    Node *n = nodeByPath(base.c_str());
    printf("base: %s, dst: %s\n",base.c_str(),dstname.c_str());
    if(!n)
        exit(10);
    handle nh = n->nodehandle;
    client->putq.push_back(new AppFilePut(filename.c_str(),nh,"",dstname.c_str()));
    megaFuseCallback.upload_lock.lock();
    engine_mutex.unlock();
    megaFuseCallback.upload_lock.lock();
    megaFuseCallback.upload_lock.unlock();*/
    return true;


}

int MegaFuse::open(const char *p, struct fuse_file_info *fi)
{
    std::lock_guard<std::mutex>lock(api_mutex);
    std::string path(p);
    //auto path = splitPath(std::string(p));
    {
        std::lock_guard<std::mutex>lock(engine_mutex);
        Node *n = nodeByPath(p);
        if(!n)
            return -ENOENT;


        if(file_cache.find(path) != file_cache.end())
           {
                printf("not implemented, open from cache\n");
                exit(1);
           }

            int td = client->topen(n->nodehandle, NULL, 0,-1, 1);
             if(td < 0)
                return -EIO;

            file_cache[path].td = td;
            file_cache[path].last_modified = n->mtime;   //client->getq.push_back(new AppFileGet(n->nodehandle));

    }

    {

        opend_ret=0;
        printf("opend: aspetto il risultato\n");
        std::unique_lock<std::mutex> lk(cvm);
        cv.wait(lk, [this]{return opend_ret;});
    }
/*
    if(opend_ret < 0)
        return -EIO;

    printf("ora ")*/
    {
        auto it = file_cache.find(std::string(p));
        std::lock_guard<std::mutex>lock(engine_mutex);

        it->second.n_clients++;
    }

    if(opend_ret < 0)
        return -EIO;

    return 0;
}

bool MegaFuse::download(std::string filename,std::string dst)
{

   /* std::lock_guard<std::mutex>lock(api_mutex);

    engine_mutex.lock();
        Node *n = nodeByPath(filename.c_str());
        if(!n)
        {
            engine_mutex.unlock();
            return false;
        }
        megaFuseCallback.open_lock.lock();
        client->getq.push_back(new AppFileGet(n->nodehandle));

    engine_mutex.unlock();
    megaFuseCallback.open_lock.lock();
    megaFuseCallback.open_lock.unlock();
    megaFuseCallback.download_lock.lock();
        engine_mutex.lock();
        client->dlopen(megaFuseCallback.result,dst.c_str());
        engine_mutex.unlock();
    megaFuseCallback.download_lock.lock();
    megaFuseCallback.download_lock.unlock();
    delete client->gettransfer((transfer_list*)&client->getq,megaFuseCallback.result);
    if(megaFuseCallback.last_error)
        return false;
*/
    return true;


}

int MegaFuse::mkdir(std::string dst)
{
 /*   std::lock_guard<std::mutex>lock(api_mutex);
    engine_mutex.lock();

                                    int pos = dst.find_last_of('/');
                                    std::string base = dst.substr(0,pos);
                                    std::string newname = dst.substr(pos+1);
                                    if(base == "")
                                        base = "/";

                                    Node* n = nodeByPath(base.c_str());
                                    SymmCipher key;
									string attrstring;
									byte buf[Node::FOLDERNODEKEYLENGTH];
									NewNode* newnode = new NewNode[1];

									// set up new node as folder node
									newnode->source = NEW_NODE;
									newnode->type = FOLDERNODE;
									newnode->nodehandle = 0;
									newnode->mtime = newnode->ctime = time(NULL);
									newnode->parenthandle = UNDEF;

									// generate fresh random key for this folder node
									PrnGen::genblock(buf,Node::FOLDERNODEKEYLENGTH);
									newnode->nodekey.assign((char*)buf,Node::FOLDERNODEKEYLENGTH);
									key.setkey(buf);

									// generate fresh attribute object with the folder name
									AttrMap attrs;
									attrs.map['n'] = newname;

									// JSON-encode object and encrypt attribute string
									attrs.getjson(&attrstring);
									client->makeattr(&key,&newnode->attrstring,attrstring.c_str());

									// add the newly generated folder node
									client->putnodes(n->nodehandle,newnode,1);
    megaFuseCallback.putnodes_lock.lock();
    engine_mutex.unlock();
    megaFuseCallback.putnodes_lock.lock();
    megaFuseCallback.putnodes_lock.unlock();
    if(megaFuseCallback.last_error)
        return -1;*/
    return 0;
}
int MegaFuse::unlink(std::string filename)
{
   /* std::lock_guard<std::mutex>lock(api_mutex);

    engine_mutex.lock();
    Node *n = nodeByPath(filename.c_str());
    if(!n)
    {
        engine_mutex.unlock();
        return -ENOENT;
    }


    error e = client->unlink(n);
    if(e)
    {
        engine_mutex.unlock();
        return -ENOENT;
    }
    megaFuseCallback.unlink_lock.lock();
    engine_mutex.unlock();
    megaFuseCallback.unlink_lock.lock();
    megaFuseCallback.unlink_lock.unlock();
    if(megaFuseCallback.last_error)
        return -EIO;*/
    return 0;

}

int MegaFuse::open(std::string filename)
{
  /*  engine_mutex.lock();
    Node *n = nodeByPath(filename.c_str());

    //megaFuseCallback.open_lock.lock();
    int res = client->topen(n->nodehandle);
    printf("topen ritorna %d\n",res);
    engine_mutex.unlock();
    if(res < 0)
    {
        //megaFuseCallback.open_lock.unlock();
        return -1;
    }
    usleep(500000);
    //megaFuseCallback.open_lock.lock();

    //megaFuseCallback.open_lock.unlock();
    return res;
    return megaFuseCallback.result;*/

}