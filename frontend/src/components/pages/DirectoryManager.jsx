import { useReducer } from "react"

function DirectoryManager({ username }) {

    let [files, setFiles] = useReducer((prev, action)=>{

    },[])

    return <p>{username}</p>
}
export default DirectoryManager
