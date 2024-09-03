Hey!  
If you're reading this, you probably got curious about why Bristlecone directly includes the GDK even though there's a good way to pull it through UE.
Well, that's because you have to build UE 5.4 from source right now to pull it in, and I don't actually want to require that people build from source
if they just want to try out bristlecone and get a feel for how it works. As a result, I'm violating quite a few best practices by packing this in.  
  
It's my hope that we'll be able to remove this quite soon, probably as soon as 5.4.1 or when GDK starts shipping the lib files in more elegant ways.