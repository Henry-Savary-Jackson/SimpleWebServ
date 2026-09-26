import { useReducer } from "react"

function DirectoryManager({ username }) {

    let [files, setFiles] = useReducer((prev, action)=>{
        switch (action.action) {
            case "FETCH":


                break;

            default:
                return prev;
        }
    },[])

    return <p>{username}</p>
}
export default DirectoryManager
